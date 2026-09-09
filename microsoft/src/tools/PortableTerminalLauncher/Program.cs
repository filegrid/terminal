using System;
using System.Diagnostics;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Text;
using System.Threading;
using System.Windows.Forms;

internal static class Program
{
    private const string FooterMagic = "WTPORT01";
    private const string MainExecutableName = "WindowsTerminal.exe";
    private const string SettingsDirectoryOverrideEnvironmentVariable = "WT_SETTINGS_DIR_OVERRIDE";
    private const string PortableRootArgument = "--portable-root";
    private const string PortableRootEnvironmentVariable = "WT_PORTABLE_ROOT";
    private const string UiLanguageOverrideEnvironmentVariable = "WT_UI_LANGUAGE_OVERRIDE";
    private const string FixedEnglishMarker = "english";
    private const string FixedEnglishLocaleMarker = "en-us";

    [STAThread]
    private static int Main(string[] args)
    {
        try
        {
            var launchOptions = ParseLaunchOptions(args);
            var launcherPath = GetLauncherPath();
            var payloadInfo = ReadPayloadInfo(launcherPath);
            var extractionRoot = EnsureExtractedPayload(launcherPath, payloadInfo, launchOptions.PortableRoot);
            var terminalPath = FindMainExecutable(extractionRoot);

            var startInfo = new ProcessStartInfo
            {
                FileName = terminalPath,
                Arguments = JoinArguments(launchOptions.TerminalArguments),
                WorkingDirectory = Environment.CurrentDirectory,
                UseShellExecute = false
            };
            ConfigurePortableDataRoot(startInfo, launchOptions.PortableRoot);
            ApplyOptionalLanguageOverride(startInfo, launcherPath);

            if (Process.Start(startInfo) == null)
            {
                throw new InvalidOperationException("Failed to start Windows Terminal.");
            }

            return 0;
        }
        catch (Exception ex)
        {
            if (!string.IsNullOrWhiteSpace(Environment.GetEnvironmentVariable(PortableRootEnvironmentVariable)))
            {
                TryWritePortableLaunchError(ex);
                return Marshal.GetHRForException(ex);
            }

            MessageBox.Show(ex.Message, "Windows Terminal Portable", MessageBoxButtons.OK, MessageBoxIcon.Error);
            return Marshal.GetHRForException(ex);
        }
    }

    private static LaunchOptions ParseLaunchOptions(string[] args)
    {
        var terminalArguments = new System.Collections.Generic.List<string>();
        string portableRoot = null;
        var parseLauncherArguments = true;

        for (var index = 0; index < args.Length; index++)
        {
            var argument = args[index];
            if (parseLauncherArguments && string.Equals(argument, "--", StringComparison.Ordinal))
            {
                parseLauncherArguments = false;
                terminalArguments.Add(argument);
                continue;
            }

            if (parseLauncherArguments && string.Equals(argument, PortableRootArgument, StringComparison.OrdinalIgnoreCase))
            {
                if (++index >= args.Length || string.IsNullOrWhiteSpace(args[index]))
                {
                    throw new ArgumentException(PortableRootArgument + " requires a directory path.");
                }

                if (portableRoot != null)
                {
                    throw new ArgumentException(PortableRootArgument + " may be specified only once.");
                }

                portableRoot = Path.GetFullPath(args[index]);
                continue;
            }

            const string portableRootArgumentPrefix = "--portable-root=";
            if (parseLauncherArguments && argument.StartsWith(portableRootArgumentPrefix, StringComparison.OrdinalIgnoreCase))
            {
                if (portableRoot != null || argument.Length == portableRootArgumentPrefix.Length)
                {
                    throw new ArgumentException(PortableRootArgument + " requires one directory path.");
                }

                portableRoot = Path.GetFullPath(argument.Substring(portableRootArgumentPrefix.Length));
                continue;
            }

            terminalArguments.Add(argument);
        }

        if (portableRoot == null)
        {
            var environmentRoot = Environment.GetEnvironmentVariable(PortableRootEnvironmentVariable);
            if (!string.IsNullOrWhiteSpace(environmentRoot))
            {
                portableRoot = Path.GetFullPath(environmentRoot);
            }
        }

        return new LaunchOptions(portableRoot, terminalArguments.ToArray());
    }

    private static void TryWritePortableLaunchError(Exception exception)
    {
        try
        {
            var root = Environment.GetEnvironmentVariable(PortableRootEnvironmentVariable);
            if (!string.IsNullOrWhiteSpace(root))
            {
                Directory.CreateDirectory(root);
                File.WriteAllText(Path.Combine(root, "launcher-error.log"), exception.ToString());
            }
        }
        catch
        {
        }
    }

    private static string GetLauncherPath()
    {
        var path = Process.GetCurrentProcess().MainModule.FileName;
        if (string.IsNullOrWhiteSpace(path))
        {
            throw new InvalidOperationException("Could not determine the launcher path.");
        }

        return path;
    }

    private static void ApplyOptionalLanguageOverride(ProcessStartInfo startInfo, string launcherPath)
    {
        var launcherBaseName = Path.GetFileNameWithoutExtension(launcherPath);
        if (string.IsNullOrWhiteSpace(launcherBaseName))
        {
            return;
        }

        var normalized = launcherBaseName.ToLowerInvariant();
        if (normalized.Contains(FixedEnglishMarker) || normalized.Contains(FixedEnglishLocaleMarker))
        {
            startInfo.EnvironmentVariables[UiLanguageOverrideEnvironmentVariable] = "en-US";
        }
    }

    private static string FindMainExecutable(string extractionRoot)
    {
        var terminalPath = Directory.GetFiles(extractionRoot, MainExecutableName, SearchOption.AllDirectories).FirstOrDefault();
        if (terminalPath == null)
        {
            throw new FileNotFoundException("Could not find the embedded WindowsTerminal.exe payload.");
        }

        return terminalPath;
    }

    private static void ConfigurePortableDataRoot(ProcessStartInfo startInfo, string portableRoot)
    {
        if (portableRoot != null)
        {
            var settingsRoot = Path.Combine(portableRoot, ".wt");
            Directory.CreateDirectory(settingsRoot);
            startInfo.EnvironmentVariables[SettingsDirectoryOverrideEnvironmentVariable] = settingsRoot;
            // Keep Windows' actual profile variables intact. Changing USERPROFILE
            // makes shell APIs look for standard profile folders that do not exist
            // under a portable root. Workspace persistence uses this explicit value.
            startInfo.EnvironmentVariables[PortableRootEnvironmentVariable] = portableRoot;
            return;
        }

        startInfo.EnvironmentVariables[SettingsDirectoryOverrideEnvironmentVariable] = Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
            "Packages",
            "Microsoft.WindowsTerminal_8wekyb3d8bbwe",
            "LocalState");
    }

    private static string EnsureExtractedPayload(string launcherPath, PayloadInfo payloadInfo, string portableRoot)
    {
        var cacheKey = ComputeCacheKey(launcherPath);
        var cacheRoot = GetCacheRoot(cacheKey, portableRoot);
        var payloadRoot = Path.Combine(cacheRoot, "p");
        var completionSentinel = Path.Combine(cacheRoot, "c");

        try
        {
            Directory.CreateDirectory(cacheRoot);
        }
        catch (UnauthorizedAccessException)
        {
            if (portableRoot == null)
            {
                return ExtractPayloadFallback(launcherPath, payloadInfo, cacheKey);
            }

            throw;
        }

        using (var mutex = new Mutex(false, @"Local\WindowsTerminalPortable_" + cacheKey))
        {
            mutex.WaitOne();
            try
            {
                if (File.Exists(completionSentinel) && File.Exists(Path.Combine(payloadRoot, "terminal.marker")))
                {
                    return payloadRoot;
                }

                var extractionPath = Path.Combine(cacheRoot, "x");
                if (Directory.Exists(extractionPath))
                {
                    Directory.Delete(extractionPath, true);
                }

                Directory.CreateDirectory(extractionPath);

                try
                {
                    var zipPath = Path.Combine(extractionPath, "p.zip");
                    CopyPayloadTo(launcherPath, payloadInfo, zipPath);

                    var unpackRoot = Path.Combine(extractionPath, "p");
                    ZipFile.ExtractToDirectory(zipPath, unpackRoot);
                    File.Delete(zipPath);
                    File.WriteAllText(Path.Combine(unpackRoot, "terminal.marker"), string.Empty);

                    if (Directory.Exists(payloadRoot))
                    {
                        Directory.Delete(payloadRoot, true);
                    }

                    Directory.Move(unpackRoot, payloadRoot);
                    File.WriteAllText(completionSentinel, DateTime.UtcNow.ToString("O"));
                }
                catch (UnauthorizedAccessException)
                {
                    // Some endpoint security products briefly deny a nested
                    // create or move below the normal portable cache. Do not
                    // turn that transient condition into a user-facing launch
                    // error: expand this invocation into an isolated fallback
                    // directory and start from there instead.
                    if (portableRoot == null)
                    {
                        return ExtractPayloadFallback(launcherPath, payloadInfo, cacheKey);
                    }

                    throw;
                }
                catch (IOException)
                {
                    if (portableRoot == null)
                    {
                        return ExtractPayloadFallback(launcherPath, payloadInfo, cacheKey);
                    }

                    throw;
                }
                finally
                {
                    try
                    {
                        if (Directory.Exists(extractionPath))
                        {
                            Directory.Delete(extractionPath, true);
                        }
                    }
                    catch (IOException) { }
                    catch (UnauthorizedAccessException) { }
                }

                return payloadRoot;
            }
            finally
            {
                mutex.ReleaseMutex();
            }
        }
    }

    private static string ExtractPayloadFallback(string launcherPath, PayloadInfo payloadInfo, string cacheKey)
    {
        var root = Path.Combine(Path.GetTempPath(), "WindowsTerminalPortable", cacheKey.Substring(0, Math.Min(12, cacheKey.Length)), Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(root);
        var zipPath = Path.Combine(root, "payload.zip");
        try
        {
            CopyPayloadTo(launcherPath, payloadInfo, zipPath);
            ZipFile.ExtractToDirectory(zipPath, root);
            File.WriteAllText(Path.Combine(root, "terminal.marker"), string.Empty);
            return root;
        }
        finally
        {
            if (File.Exists(zipPath))
            {
                File.Delete(zipPath);
            }
        }
    }

    private static void CopyPayloadTo(string launcherPath, PayloadInfo payloadInfo, string zipPath)
    {
        using (var source = new FileStream(launcherPath, FileMode.Open, FileAccess.Read, FileShare.Read))
        using (var destination = new FileStream(zipPath, FileMode.Create, FileAccess.Write, FileShare.None))
        {
            source.Position = payloadInfo.Offset;

            var buffer = new byte[81920];
            var remaining = payloadInfo.Length;
            while (remaining > 0)
            {
                var toRead = (int)Math.Min(buffer.Length, remaining);
                var read = source.Read(buffer, 0, toRead);
                if (read == 0)
                {
                    throw new EndOfStreamException("The embedded payload ended unexpectedly.");
                }

                destination.Write(buffer, 0, read);
                remaining -= read;
            }
        }
    }

    private static string ComputeCacheKey(string launcherPath)
    {
        var fileInfo = new FileInfo(launcherPath);
        var keyMaterial = launcherPath + "|" + fileInfo.Length + "|" + fileInfo.LastWriteTimeUtc.Ticks;
        using (var sha = SHA256.Create())
        {
            var hash = sha.ComputeHash(Encoding.UTF8.GetBytes(keyMaterial));
            return BitConverter.ToString(hash).Replace("-", string.Empty);
        }
    }

    private static string GetCacheRoot(string cacheKey, string portableRoot)
    {
        const int CacheKeyPrefixLength = 12;
        var shortCacheKey = cacheKey.Length > CacheKeyPrefixLength ?
            cacheKey.Substring(0, CacheKeyPrefixLength) :
            cacheKey;

        if (portableRoot != null)
        {
            return Path.Combine(portableRoot, "payload", shortCacheKey);
        }

        return Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
            "WTP",
            shortCacheKey);
    }

    private static PayloadInfo ReadPayloadInfo(string launcherPath)
    {
        using (var stream = new FileStream(launcherPath, FileMode.Open, FileAccess.Read, FileShare.Read))
        {
            if (stream.Length < 16)
            {
                throw new InvalidDataException("This launcher does not contain an embedded payload.");
            }

            stream.Position = stream.Length - 16;
            var footer = new byte[16];
            ReadExactly(stream, footer, footer.Length);

            var payloadLength = BitConverter.ToInt64(footer, 0);
            var magic = Encoding.ASCII.GetString(footer, 8, 8);
            if (!string.Equals(magic, FooterMagic, StringComparison.Ordinal) ||
                payloadLength <= 0 ||
                payloadLength > stream.Length - 16)
            {
                throw new InvalidDataException("This launcher contains an invalid embedded payload.");
            }

            return new PayloadInfo(stream.Length - 16 - payloadLength, payloadLength);
        }
    }

    private static void ReadExactly(Stream stream, byte[] buffer, int count)
    {
        var offset = 0;
        while (count > 0)
        {
            var read = stream.Read(buffer, offset, count);
            if (read == 0)
            {
                throw new EndOfStreamException("Unexpected end of file while reading the payload footer.");
            }

            offset += read;
            count -= read;
        }
    }

    private static string JoinArguments(string[] args)
    {
        return string.Join(" ", args.Select(QuoteArgument));
    }

    private static string QuoteArgument(string argument)
    {
        if (string.IsNullOrEmpty(argument))
        {
            return "\"\"";
        }

        if (!argument.Any(ch => char.IsWhiteSpace(ch) || ch == '"'))
        {
            return argument;
        }

        var builder = new StringBuilder();
        builder.Append('"');
        var backslashCount = 0;
        foreach (var ch in argument)
        {
            if (ch == '\\')
            {
                backslashCount++;
                continue;
            }

            if (ch == '"')
            {
                builder.Append('\\', backslashCount * 2 + 1);
                builder.Append('"');
                backslashCount = 0;
                continue;
            }

            if (backslashCount > 0)
            {
                builder.Append('\\', backslashCount);
                backslashCount = 0;
            }

            builder.Append(ch);
        }

        if (backslashCount > 0)
        {
            builder.Append('\\', backslashCount * 2);
        }

        builder.Append('"');
        return builder.ToString();
    }

    private struct PayloadInfo
    {
        public PayloadInfo(long offset, long length)
        {
            Offset = offset;
            Length = length;
        }

        public long Offset { get; private set; }
        public long Length { get; private set; }
    }

    private struct LaunchOptions
    {
        public LaunchOptions(string portableRoot, string[] terminalArguments)
        {
            PortableRoot = portableRoot;
            TerminalArguments = terminalArguments;
        }

        public string PortableRoot { get; private set; }
        public string[] TerminalArguments { get; private set; }
    }
}
