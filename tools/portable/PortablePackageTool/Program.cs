using System.Diagnostics;
using System.IO.Compression;
using System.Security.Cryptography;
using System.Text;
using System.Xml;
using System.Xml.Linq;

internal sealed class Program
{
    private const string FooterMagic = "WTPORT01";
    private const string DefaultMakePriPath = @"C:\Program Files (x86)\Windows Kits\10\bin\10.0.22621.0\x64\MakePri.exe";
    private const string CacheFormatVersion = "portable-cache-v2";

    private static int Main(string[] args)
    {
        try
        {
            if (args.Length > 0 && string.Equals(args[0], "generate-manifest", StringComparison.Ordinal))
            {
                ProductManifestGenerator.Generate(args[1..]);
                return 0;
            }

            if (args.Length > 0 && string.Equals(args[0], "generate-pri-config", StringComparison.Ordinal))
            {
                ProductPriGenerator.Generate(args[1..]);
                return 0;
            }

            if (args.Length > 0 && string.Equals(args[0], "generate-package-map", StringComparison.Ordinal))
            {
                ProductPackageMapGenerator.Generate(args[1..]);
                return 0;
            }

            var options = Options.Parse(args);
            new PortablePackageTool(options).Run();
            return 0;
        }
        catch (Exception ex)
        {
            Console.Error.WriteLine(ex.Message);
            return 1;
        }
    }

    private sealed class PortablePackageTool
    {
        private readonly Options _options;

        public PortablePackageTool(Options options)
        {
            _options = options;
        }

        public void Run()
        {
            Directory.CreateDirectory(_options.Destination);
            var package = new FileInfo(_options.PackagePath);
            if (!package.Exists)
            {
                throw new InvalidOperationException($"Could not find the terminal package at {package.FullName}.");
            }
            var xamlPackage = new FileInfo(_options.XamlPackagePath);
            if (!xamlPackage.Exists)
            {
                throw new InvalidOperationException($"Could not find the Microsoft.UI.Xaml package at {xamlPackage.FullName}.");
            }

            var tempRoot = Path.Combine(Path.GetTempPath(), "wt-portable-" + Guid.NewGuid().ToString("N"));
            Directory.CreateDirectory(tempRoot);

            try
            {
                var manifest = ReadPackageManifest(package.FullName);
                var xamlManifest = ReadPackageManifest(xamlPackage.FullName);
                var version = manifest.Version;
                var architecture = manifest.ProcessorArchitecture;
                var distributionName = $"{manifest.Name}_{version}_{architecture}";
                var terminalDirectoryName = $"terminal-{version}";

                if (!string.Equals(xamlManifest.ProcessorArchitecture, architecture, StringComparison.OrdinalIgnoreCase))
                {
                    throw new InvalidOperationException($"{xamlPackage.FullName} is not built for {architecture}.");
                }

                var baseZip = EnsureBaseDistributionZip(package, xamlPackage, terminalDirectoryName);
                var outputZip = Path.Combine(tempRoot, distributionName + ".zip");
                File.Copy(baseZip, outputZip, true);
                OverlayWorkspaceExtensionRuntime(outputZip, terminalDirectoryName);

                if (_options.SingleFileOutput)
                {
                    BuildSingleFilePortable(distributionName, version, architecture, outputZip);
                }
                else
                {
                    var outputZipPath = Path.Combine(_options.Destination, distributionName + ".zip");
                    if (File.Exists(outputZipPath))
                    {
                        File.Delete(outputZipPath);
                    }

                    File.Copy(outputZip, outputZipPath);
                }
            }
            finally
            {
                if (Directory.Exists(tempRoot))
                {
                    Directory.Delete(tempRoot, true);
                }
            }
        }

        private string EnsureBaseDistributionZip(FileInfo package, FileInfo xamlPackage, string terminalDirectoryName)
        {
            var cacheRoot = GetCacheRoot();
            Directory.CreateDirectory(cacheRoot);

            var layoutNeedsPortableFiles = _options.PortableMode && !_options.SingleFileOutput;
            var cacheKey = ComputeCacheKey(package, xamlPackage, layoutNeedsPortableFiles);
            var cacheDirectory = Path.Combine(cacheRoot, cacheKey);
            var baseZip = Path.Combine(cacheDirectory, "base.zip");
            if (File.Exists(baseZip))
            {
                return baseZip;
            }

            var stagingDirectory = Path.Combine(cacheRoot, cacheKey + ".build-" + Guid.NewGuid().ToString("N"));
            Directory.CreateDirectory(stagingDirectory);

            try
            {
                var terminalRoot = Path.Combine(stagingDirectory, terminalDirectoryName);
                var xamlRoot = Path.Combine(stagingDirectory, "xaml");
                ZipFile.ExtractToDirectory(package.FullName, terminalRoot);
                ZipFile.ExtractToDirectory(xamlPackage.FullName, xamlRoot);

                PrepareTerminalLayout(terminalRoot, xamlRoot);
                MergeResources(terminalRoot, xamlRoot);

                if (layoutNeedsPortableFiles)
                {
                    CreatePortableModeFiles(terminalRoot);
                }

                var stagedZip = Path.Combine(stagingDirectory, "base.zip");
                CreateDistributionZip(terminalRoot, terminalDirectoryName, stagedZip);

                Directory.CreateDirectory(cacheDirectory);
                File.Copy(stagedZip, baseZip, true);
                return baseZip;
            }
            finally
            {
                if (Directory.Exists(stagingDirectory))
                {
                    Directory.Delete(stagingDirectory, true);
                }
            }
        }

        private string GetCacheRoot()
        {
            return Path.Combine(_options.SourceRoot, "microsoft", "tmp", "portable-package-cache");
        }

        private string ComputeCacheKey(FileInfo package, FileInfo xamlPackage, bool layoutNeedsPortableFiles)
        {
            var payload = string.Join('|',
            [
                CacheFormatVersion,
                package.FullName,
                package.Length.ToString(),
                package.LastWriteTimeUtc.Ticks.ToString(),
                xamlPackage.FullName,
                xamlPackage.Length.ToString(),
                xamlPackage.LastWriteTimeUtc.Ticks.ToString(),
                _options.Platform,
                _options.Configuration,
                layoutNeedsPortableFiles ? "portable-files" : "plain-layout"
            ]);

            var bytes = SHA256.HashData(Encoding.UTF8.GetBytes(payload));
            return Convert.ToHexString(bytes);
        }

        private PackageManifest ReadPackageManifest(string packagePath)
        {
            using var archive = ZipFile.OpenRead(packagePath);
            var manifestEntry = archive.GetEntry("AppxManifest.xml") ?? throw new InvalidOperationException($"Could not find AppxManifest.xml in {packagePath}.");
            using var stream = manifestEntry.Open();
            var document = XDocument.Load(stream);
            var identity = document.Descendants()
                .FirstOrDefault(element => string.Equals(element.Name.LocalName, "Identity", StringComparison.Ordinal))
                ?? throw new InvalidOperationException($"Could not read package identity from {packagePath}.");
            return new PackageManifest(
                identity.Attribute("Name")?.Value ?? string.Empty,
                identity.Attribute("Version")?.Value ?? string.Empty,
                identity.Attribute("ProcessorArchitecture")?.Value ?? string.Empty);
        }

        private void PrepareTerminalLayout(string terminalRoot, string xamlRoot)
        {
            foreach (var file in Directory.EnumerateFiles(terminalRoot, "*.xml", SearchOption.TopDirectoryOnly))
            {
                File.Delete(file);
            }

            foreach (var file in Directory.EnumerateFiles(terminalRoot, "*.winmd", SearchOption.TopDirectoryOnly))
            {
                File.Delete(file);
            }

            foreach (var path in Directory.EnumerateFileSystemEntries(terminalRoot, "Appx*", SearchOption.TopDirectoryOnly))
            {
                DeletePath(path);
            }

            var imagesDirectory = Path.Combine(terminalRoot, "Images");
            if (Directory.Exists(imagesDirectory))
            {
                foreach (var file in Directory.EnumerateFiles(imagesDirectory, "*Tile*", SearchOption.TopDirectoryOnly))
                {
                    File.Delete(file);
                }

                foreach (var file in Directory.EnumerateFiles(imagesDirectory, "*Logo*", SearchOption.TopDirectoryOnly))
                {
                    File.Delete(file);
                }
            }

            var xamlDll = Path.Combine(xamlRoot, "Microsoft.UI.Xaml.dll");
            File.Copy(xamlDll, Path.Combine(terminalRoot, "Microsoft.UI.Xaml.dll"), true);

            var xamlDirectory = Path.Combine(xamlRoot, "Microsoft.UI.Xaml");
            CopyDirectory(xamlDirectory, Path.Combine(terminalRoot, "Microsoft.UI.Xaml"));
        }

        private void MergeResources(string terminalRoot, string xamlRoot)
        {
            var tempRoot = Path.Combine(Path.GetTempPath(), "wt-portable-pri-" + Guid.NewGuid().ToString("N"));
            Directory.CreateDirectory(tempRoot);

            try
            {
                var terminalDump = Path.Combine(tempRoot, "terminal.pri.xml");
                RunProcess(_options.MakePriPath, $"dump /if \"{Path.Combine(terminalRoot, "resources.pri")}\" /of \"{terminalDump}\" /dt detailed");

                var terminalDumpXml = new XmlDocument();
                terminalDumpXml.Load(terminalDump);
                RemoveXamlSubtree(terminalDumpXml);
                terminalDumpXml.Save(terminalDump);

                var indexName = terminalDumpXml.GetElementsByTagName("ResourceMap")
                    .OfType<XmlElement>()
                    .Select(element => element.GetAttribute("name"))
                    .FirstOrDefault(value => !string.IsNullOrWhiteSpace(value)) ?? "Application";

                var priConfigPath = Path.Combine(tempRoot, "priconfig.xml");
                var priListPath = Path.Combine(tempRoot, "pri.resfiles");
                var dumpListPath = Path.Combine(tempRoot, "dump.resfiles");

                File.WriteAllText(priConfigPath, PriConfigContent, new UTF8Encoding(false));
                File.WriteAllLines(priListPath, new[] { Path.Combine(xamlRoot, "resources.pri") }, new UTF8Encoding(false));
                File.WriteAllLines(dumpListPath, new[] { terminalDump }, new UTF8Encoding(false));

                RunProcess(_options.MakePriPath, $"new /pr \"{tempRoot}\" /cf \"{priConfigPath}\" /o /in {indexName} /of \"{Path.Combine(terminalRoot, "resources.pri")}\"");
            }
            finally
            {
                if (Directory.Exists(tempRoot))
                {
                    Directory.Delete(tempRoot, true);
                }
            }
        }

        private void CreatePortableModeFiles(string terminalRoot)
        {
            File.WriteAllText(Path.Combine(terminalRoot, ".portable"), string.Empty);
            var settingsDirectory = Path.Combine(terminalRoot, "settings");
            Directory.CreateDirectory(settingsDirectory);
            File.WriteAllText(Path.Combine(settingsDirectory, "settings.json"), "{\n  \"profiles\": {\n    \"defaults\": {\n      \"cursorShape\": \"vintage\"\n    }\n  }\n}\n", new UTF8Encoding(true));
        }

        private void OverlayWorkspaceExtensionRuntime(string outputZip, string terminalDirectoryName)
        {
            using var archive = ZipFile.Open(outputZip, ZipArchiveMode.Update);

            if (!string.IsNullOrWhiteSpace(_options.WorkspaceExtensionOutputDirectory))
            {
                var outputDirectory = _options.WorkspaceExtensionOutputDirectory;
                if (!Directory.Exists(outputDirectory))
                {
                    throw new InvalidOperationException($"Workspace extension output directory {outputDirectory} does not exist.");
                }

                UpdateZipEntryIfPresent(archive, outputDirectory, terminalDirectoryName, "Ext.dll");
                UpdateZipEntryIfPresent(archive, outputDirectory, terminalDirectoryName, "Glue.dll");
                OverlayTerminalUiRuntime(archive, terminalDirectoryName);
                OverlayWorkspaceSpriteResources(archive, terminalDirectoryName, outputDirectory);
                OverlayMergedIconAtlases(archive, terminalDirectoryName, outputDirectory);
                OverlayWebResources(archive, terminalDirectoryName);
                OverlayWebView2Loader(archive, terminalDirectoryName);
                return;
            }

            OverlayTerminalUiRuntime(archive, terminalDirectoryName);
            OverlayWorkspaceSpriteResources(archive, terminalDirectoryName, string.Empty);
            OverlayMergedIconAtlases(archive, terminalDirectoryName, string.Empty);
            OverlayWebResources(archive, terminalDirectoryName);
            OverlayWebView2Loader(archive, terminalDirectoryName);
        }

        private void OverlayWebResources(ZipArchive archive, string terminalDirectoryName)
        {
            var sourceDirectory = Path.Combine(_options.SourceRoot, "res", "web");
            if (!Directory.Exists(sourceDirectory))
            {
                return;
            }

            foreach (var file in Directory.EnumerateFiles(sourceDirectory, "*", SearchOption.AllDirectories))
            {
                var relativePath = Path.GetRelativePath(sourceDirectory, file).Replace('\\', '/');
                var entryName = $"{terminalDirectoryName}/res/web/{relativePath}";
                archive.GetEntry(entryName)?.Delete();
                archive.CreateEntryFromFile(file, entryName, CompressionLevel.Optimal);
            }
        }

        private void OverlayWebView2Loader(ZipArchive archive, string terminalDirectoryName)
        {
            var sourceDirectory = Path.Combine(
                _options.SourceRoot,
                "microsoft",
                "packages",
                "Microsoft.Web.WebView2.1.0.1661.34",
                "runtimes",
                "win-" + _options.Platform,
                "native");
            UpdateZipEntryIfPresent(archive, sourceDirectory, terminalDirectoryName, "WebView2Loader.dll");
            var uapSourceDirectory = Path.Combine(
                _options.SourceRoot,
                "microsoft",
                "packages",
                "Microsoft.Web.WebView2.1.0.1661.34",
                "runtimes",
                "win-" + _options.Platform,
                "native_uap");
            UpdateZipEntryIfPresent(archive, uapSourceDirectory, terminalDirectoryName, "Microsoft.Web.WebView2.Core.dll");
        }

        private void OverlayTerminalUiRuntime(ZipArchive archive, string terminalDirectoryName)
        {
            foreach (var candidateDirectory in GetTerminalUiRuntimeDirectories())
            {
                if (!Directory.Exists(candidateDirectory))
                {
                    continue;
                }

                var sourcePath = Path.Combine(candidateDirectory, "Microsoft.Terminal.UI.dll");
                if (!File.Exists(sourcePath))
                {
                    continue;
                }

                UpdateZipEntryIfPresent(archive, candidateDirectory, terminalDirectoryName, "Microsoft.Terminal.UI.dll");
                return;
            }
        }

        private void OverlayWorkspaceSpriteResources(ZipArchive archive, string terminalDirectoryName, string outputDirectory)
        {
            var candidateDirectories = new[]
            {
                Path.Combine(outputDirectory, "res"),
                Path.Combine(_options.SourceRoot, "bin", "res")
            };

            foreach (var candidateDirectory in candidateDirectories.Distinct(StringComparer.OrdinalIgnoreCase))
            {
                if (!Directory.Exists(candidateDirectory))
                {
                    continue;
                }

                var svgRoot = Path.Combine(candidateDirectory, "workspace-icons");
                if (Directory.Exists(svgRoot))
                {
                    foreach (var file in Directory.EnumerateFiles(svgRoot, "*.svg", SearchOption.AllDirectories))
                    {
                        var relativePath = Path.GetRelativePath(candidateDirectory, file).Replace('\\', '/');
                        var entryName = $"{terminalDirectoryName}/res/{relativePath}".Replace('\\', '/');
                        archive.GetEntry(entryName)?.Delete();
                        archive.CreateEntryFromFile(file, entryName, CompressionLevel.Optimal);
                    }
                }

                foreach (var file in Directory.EnumerateFiles(candidateDirectory, "workspace-icons-*.png", SearchOption.TopDirectoryOnly))
                {
                    var entryName = $"{terminalDirectoryName}/res/{Path.GetFileName(file)}".Replace('\\', '/');
                    archive.GetEntry(entryName)?.Delete();
                    archive.CreateEntryFromFile(file, entryName, CompressionLevel.Optimal);
                }

                return;
            }
        }

        private void OverlayMergedIconAtlases(ZipArchive archive, string terminalDirectoryName, string outputDirectory)
        {
            foreach (var candidateDirectory in GetMergedIconAtlasDirectories(outputDirectory))
            {
                if (!Directory.Exists(candidateDirectory))
                {
                    continue;
                }

                foreach (var file in Directory.EnumerateFiles(candidateDirectory, "merged.png", SearchOption.AllDirectories))
                {
                    var relativePath = Path.GetRelativePath(candidateDirectory, file).Replace('\\', '/');
                    var entryName = $"{terminalDirectoryName}/res/{relativePath}".Replace('\\', '/');
                    archive.GetEntry(entryName)?.Delete();
                    archive.CreateEntryFromFile(file, entryName, CompressionLevel.Optimal);
                }

                return;
            }
        }

        private IEnumerable<string> GetTerminalUiRuntimeDirectories()
        {
            yield return Path.Combine(_options.SourceRoot, "microsoft", "bin", _options.Platform, _options.Configuration, "Microsoft.Terminal.UI");
            yield return Path.Combine(_options.SourceRoot, "bin");
        }

        private IEnumerable<string> GetMergedIconAtlasDirectories(string outputDirectory)
        {
            if (!string.IsNullOrWhiteSpace(outputDirectory))
            {
                yield return Path.Combine(outputDirectory, "res");
            }

            // Workspace icon atlases are checked-in product resources. Do not
            // depend on a generated bin/ext copy: the portable package must
            // always take the canonical files directly from res/v1/assets.
            yield return Path.Combine(_options.SourceRoot, "res");
            yield return Path.Combine(_options.SourceRoot, "ext", "res");
            yield return Path.Combine(_options.SourceRoot, "bin", "res");
        }

        private void CreateDistributionZip(string terminalRoot, string terminalDirectoryName, string outputZip)
        {
            if (File.Exists(outputZip))
            {
                File.Delete(outputZip);
            }

            using var zipStream = new FileStream(outputZip, FileMode.CreateNew, FileAccess.Write, FileShare.None);
            using var archive = new ZipArchive(zipStream, ZipArchiveMode.Create);

            foreach (var file in Directory.EnumerateFiles(terminalRoot, "*", SearchOption.AllDirectories))
            {
                var relativePath = Path.GetRelativePath(terminalRoot, file).Replace('\\', '/');
                archive.CreateEntryFromFile(file, $"{terminalDirectoryName}/{relativePath}", CompressionLevel.Optimal);
            }
        }

        private void BuildSingleFilePortable(string distributionName, string version, string architecture, string outputZip)
        {
            var launcherSource = Path.Combine(_options.SourceRoot, "microsoft", "src", "tools", "PortableTerminalLauncher", "Program.cs");
            var launcherIcon = Path.Combine(_options.SourceRoot, "microsoft", "res", "terminal.ico");
            var publishDirectory = Path.Combine(Path.GetTempPath(), "wt-portable-launcher-" + Guid.NewGuid().ToString("N"));
            Directory.CreateDirectory(publishDirectory);

            try
            {
                var assemblyInfo = Path.Combine(publishDirectory, "PortableTerminalLauncher.AssemblyInfo.cs");
                File.WriteAllText(assemblyInfo, $"using System.Reflection;{Environment.NewLine}[assembly: AssemblyTitle(\"Windows Terminal Portable\")]{Environment.NewLine}[assembly: AssemblyProduct(\"Windows Terminal Portable\")]{Environment.NewLine}[assembly: AssemblyVersion(\"{version}\")]{Environment.NewLine}[assembly: AssemblyFileVersion(\"{version}\")]{Environment.NewLine}[assembly: AssemblyInformationalVersion(\"{version}\")]{Environment.NewLine}", Encoding.ASCII);

                var launcherPath = Path.Combine(publishDirectory, "PortableTerminalLauncher.exe");
                var cscPlatform = architecture.ToLowerInvariant() switch
                {
                    "x64" => "x64",
                    "x86" => "x86",
                    "arm64" => "arm64",
                    _ => throw new InvalidOperationException($"Unsupported architecture {architecture} for single-file portable output.")
                };

                var dotnetPath = Environment.ProcessPath ?? throw new InvalidOperationException("Could not locate the current dotnet host.");
                RunProcess(dotnetPath,
                    $"\"{_options.RoslynPath}\" /noconfig /nostdlib+ /nologo /target:winexe /optimize+ /platform:{cscPlatform} /win32icon:\"{launcherIcon}\" /lib:\"{_options.FrameworkReferencePath}\" /r:mscorlib.dll /r:System.dll /r:System.Core.dll /r:System.IO.Compression.dll /r:System.IO.Compression.FileSystem.dll /r:System.Windows.Forms.dll /out:\"{launcherPath}\" \"{assemblyInfo}\" \"{launcherSource}\"");

                var outputExe = Path.Combine(_options.Destination, GetPortableOutputName(version, architecture, ".exe"));
                DeleteLegacyPortableOutputs(distributionName, version, architecture, ".exe");
                if (File.Exists(outputExe))
                {
                    File.Delete(outputExe);
                }

                File.Copy(launcherPath, outputExe);
                AppendPayload(outputExe, outputZip);
            }
            finally
            {
                if (Directory.Exists(publishDirectory))
                {
                    Directory.Delete(publishDirectory, true);
                }
            }
        }

        private void AppendPayload(string outputExe, string outputZip)
        {
            var payloadLengthBytes = BitConverter.GetBytes(new FileInfo(outputZip).Length);
            var footerBytes = Encoding.ASCII.GetBytes(FooterMagic);

            using var outputStream = new FileStream(outputExe, FileMode.Append, FileAccess.Write, FileShare.Read);
            using var payloadStream = new FileStream(outputZip, FileMode.Open, FileAccess.Read, FileShare.Read);
            payloadStream.CopyTo(outputStream);
            outputStream.Write(payloadLengthBytes, 0, payloadLengthBytes.Length);
            outputStream.Write(footerBytes, 0, footerBytes.Length);
        }

        private string GetPortableOutputName(string version, string architecture, string extension)
        {
            var configurationSuffix = string.Equals(_options.Configuration, "Release", StringComparison.OrdinalIgnoreCase) ? string.Empty : "_" + _options.Configuration;
            return $"WindowsTerminalPortableGeekEdition_System{configurationSuffix}_{version}_{architecture}{extension}";
        }

        private void DeleteLegacyPortableOutputs(string distributionName, string version, string architecture, string extension)
        {
            var configurationSuffix = string.Equals(_options.Configuration, "Release", StringComparison.OrdinalIgnoreCase) ? string.Empty : "_" + _options.Configuration;
            var legacyNames = new[]
            {
                distributionName + extension,
                $"WindowsTerminalPortable_en-US{configurationSuffix}_{version}_{architecture}{extension}",
                $"WindowsTerminalPortable_zh-CN{configurationSuffix}_{version}_{architecture}{extension}",
                $"WindowsTerminalPortableGeekEdition_System{configurationSuffix}_{version}_{architecture}{extension}",
                $"WindowsTerminalPortableGeekEdition_English{configurationSuffix}_{version}_{architecture}{extension}"
            };

            foreach (var name in legacyNames.Distinct(StringComparer.OrdinalIgnoreCase))
            {
                var outputPath = Path.Combine(_options.Destination, name);
                if (File.Exists(outputPath))
                {
                    File.Delete(outputPath);
                }
            }
        }

        private static void RemoveXamlSubtree(XmlDocument document)
        {
            var fileSubtrees = document.GetElementsByTagName("ResourceMapSubtree");
            XmlNode? filesNode = null;
            foreach (XmlNode node in fileSubtrees)
            {
                var name = node.Attributes?["name"]?.Value ?? node.Attributes?["Name"]?.Value;
                if (string.Equals(name, "Files", StringComparison.Ordinal))
                {
                    filesNode = node;
                    break;
                }
            }

            if (filesNode == null)
            {
                return;
            }

            foreach (XmlNode child in filesNode.ChildNodes)
            {
                var name = child.Attributes?["name"]?.Value ?? child.Attributes?["Name"]?.Value;
                if (string.Equals(name, "Microsoft.UI.Xaml", StringComparison.Ordinal))
                {
                    filesNode.RemoveChild(child);
                    break;
                }
            }
        }

        private static void RunProcess(string fileName, string arguments)
        {
            var startInfo = new ProcessStartInfo
            {
                FileName = fileName,
                Arguments = arguments,
                RedirectStandardOutput = true,
                RedirectStandardError = true,
                UseShellExecute = false,
                CreateNoWindow = true
            };

            using var process = Process.Start(startInfo) ?? throw new InvalidOperationException($"Failed to start {fileName}.");
            var standardOutput = process.StandardOutput.ReadToEnd();
            var standardError = process.StandardError.ReadToEnd();
            process.WaitForExit();

            if (process.ExitCode != 0)
            {
                throw new InvalidOperationException($"{fileName} failed with code {process.ExitCode}.{Environment.NewLine}{standardOutput}{standardError}");
            }
        }

        private static void CopyDirectory(string sourceDirectory, string destinationDirectory)
        {
            Directory.CreateDirectory(destinationDirectory);

            foreach (var directory in Directory.EnumerateDirectories(sourceDirectory, "*", SearchOption.AllDirectories))
            {
                Directory.CreateDirectory(Path.Combine(destinationDirectory, Path.GetRelativePath(sourceDirectory, directory)));
            }

            foreach (var file in Directory.EnumerateFiles(sourceDirectory, "*", SearchOption.AllDirectories))
            {
                var destinationPath = Path.Combine(destinationDirectory, Path.GetRelativePath(sourceDirectory, file));
                Directory.CreateDirectory(Path.GetDirectoryName(destinationPath)!);
                File.Copy(file, destinationPath, true);
            }
        }

        private static void CopyRuntimeFileIfPresent(string sourceDirectory, string destinationDirectory, string fileName)
        {
            var sourcePath = Path.Combine(sourceDirectory, fileName);
            if (!File.Exists(sourcePath))
            {
                return;
            }

            File.Copy(sourcePath, Path.Combine(destinationDirectory, fileName), true);
        }

        private static void UpdateZipEntryIfPresent(ZipArchive archive, string sourceDirectory, string terminalDirectoryName, string fileName)
        {
            var sourcePath = Path.Combine(sourceDirectory, fileName);
            if (!File.Exists(sourcePath))
            {
                return;
            }

            var entryName = $"{terminalDirectoryName}/{fileName}".Replace('\\', '/');
            archive.GetEntry(entryName)?.Delete();

            var entry = archive.CreateEntry(entryName, CompressionLevel.Optimal);
            using var input = new FileStream(sourcePath, FileMode.Open, FileAccess.Read, FileShare.Read);
            using var output = entry.Open();
            input.CopyTo(output);
        }

        private static void DeletePath(string path)
        {
            if (Directory.Exists(path))
            {
                Directory.Delete(path, true);
            }
            else if (File.Exists(path))
            {
                File.Delete(path);
            }
        }

        private const string PriConfigContent = """
<?xml version="1.0" encoding="utf-8"?>
<resources targetOsVersion="10.0.0" majorVersion="1">
  <index root="\" startIndexAt="dump.resfiles">
    <default>
      <qualifier name="Language" value="en-US" />
      <qualifier name="Contrast" value="standard" />
      <qualifier name="Scale" value="200" />
      <qualifier name="HomeRegion" value="001" />
      <qualifier name="TargetSize" value="256" />
      <qualifier name="LayoutDirection" value="LTR" />
      <qualifier name="DXFeatureLevel" value="DX9" />
      <qualifier name="Configuration" value="" />
      <qualifier name="AlternateForm" value="" />
      <qualifier name="Platform" value="UAP" />
    </default>
    <indexer-config type="PRIINFO" />
    <indexer-config type="RESFILES" qualifierDelimiter="." />
  </index>
  <index root="\" startIndexAt="pri.resfiles">
    <default>
      <qualifier name="Language" value="en-US" />
      <qualifier name="Contrast" value="standard" />
      <qualifier name="Scale" value="200" />
      <qualifier name="HomeRegion" value="001" />
      <qualifier name="TargetSize" value="256" />
      <qualifier name="LayoutDirection" value="LTR" />
      <qualifier name="DXFeatureLevel" value="DX9" />
      <qualifier name="Configuration" value="" />
      <qualifier name="AlternateForm" value="" />
      <qualifier name="Platform" value="UAP" />
    </default>
    <indexer-config type="PRI" />
    <indexer-config type="RESFILES" qualifierDelimiter="." />
  </index>
</resources>
""";
    }

    private sealed class Options
    {
        public required string PackagePath { get; init; }
        public required string XamlPackagePath { get; init; }
        public required string Platform { get; init; }
        public required string Configuration { get; init; }
        public required string Destination { get; init; }
        public required string SourceRoot { get; init; }
        public required string MakePriPath { get; init; }
        public required string RoslynPath { get; init; }
        public required string FrameworkReferencePath { get; init; }
        public string? WorkspaceExtensionOutputDirectory { get; init; }
        public required bool PortableMode { get; init; }
        public required bool SingleFileOutput { get; init; }

        public static Options Parse(string[] args)
        {
            var values = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
            var flags = new HashSet<string>(StringComparer.OrdinalIgnoreCase);

            for (var index = 0; index < args.Length; index++)
            {
                var argument = args[index];
                if (!argument.StartsWith("--", StringComparison.Ordinal))
                {
                    throw new ArgumentException($"Unexpected argument: {argument}");
                }

                if (index + 1 < args.Length && !args[index + 1].StartsWith("--", StringComparison.Ordinal))
                {
                    values[argument] = args[++index];
                }
                else
                {
                    flags.Add(argument);
                }
            }

            return new Options
            {
                PackagePath = GetRequired(values, "--package"),
                XamlPackagePath = GetRequired(values, "--xaml-package"),
                Platform = GetRequired(values, "--platform"),
                Configuration = GetRequired(values, "--configuration"),
                Destination = GetRequired(values, "--destination"),
                SourceRoot = GetRequired(values, "--source-root"),
                MakePriPath = values.TryGetValue("--makepri-path", out var makePriPath) ? makePriPath : DefaultMakePriPath,
                RoslynPath = GetRequired(values, "--roslyn-path"),
                FrameworkReferencePath = GetRequired(values, "--framework-reference-path"),
                WorkspaceExtensionOutputDirectory = values.TryGetValue("--workspace-extension-output-dir", out var workspaceExtensionOutputDirectory) ? workspaceExtensionOutputDirectory : null,
                PortableMode = flags.Contains("--portable-mode"),
                SingleFileOutput = flags.Contains("--single-file-output")
            };
        }

        private static string GetRequired(IReadOnlyDictionary<string, string> values, string key)
        {
            return values.TryGetValue(key, out var value) && !string.IsNullOrWhiteSpace(value)
                ? value
                : throw new ArgumentException($"Missing required argument {key}");
        }
    }

    private sealed record PackageManifest(string Name, string Version, string ProcessorArchitecture);
}
