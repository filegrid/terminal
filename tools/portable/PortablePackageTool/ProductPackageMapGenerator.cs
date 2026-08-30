using System.Text;

internal static class ProductPackageMapGenerator
{
    private static readonly (string Source, string Target)[] ProductPayload =
    {
        (@"Microsoft.Terminal.Control\Microsoft.Terminal.Control.dll", "Microsoft.Terminal.Control.dll"),
        (@"TerminalConnection\TerminalConnection.dll", "TerminalConnection.dll"),
        (@"Microsoft.Terminal.UI\Microsoft.Terminal.UI.dll", "Microsoft.Terminal.UI.dll"),
        (@"Microsoft.Terminal.Settings.Model\Microsoft.Terminal.Settings.Model.dll", "Microsoft.Terminal.Settings.Model.dll"),
        (@"TerminalApp\TerminalApp.dll", "TerminalApp.dll"),
        (@"Microsoft.Terminal.Settings.Editor\Microsoft.Terminal.Settings.Editor.dll", "Microsoft.Terminal.Settings.Editor.dll"),
        (@"Microsoft.Terminal.UI.Markdown\Microsoft.Terminal.UI.Markdown.dll", "Microsoft.Terminal.UI.Markdown.dll"),
        (@"WindowsTerminalShellExt\WindowsTerminalShellExt.dll", "WindowsTerminalShellExt.dll"),
        (@"Microsoft.Terminal.Control\Microsoft.Terminal.Control.winmd", "Microsoft.Terminal.Control.winmd"),
        (@"TerminalConnection\Microsoft.Terminal.TerminalConnection.winmd", "Microsoft.Terminal.TerminalConnection.winmd"),
        (@"Microsoft.Terminal.UI\Microsoft.Terminal.UI.winmd", "Microsoft.Terminal.UI.winmd"),
        (@"Microsoft.Terminal.Settings.Model\Microsoft.Terminal.Settings.Model.winmd", "Microsoft.Terminal.Settings.Model.winmd"),
        (@"TerminalApp\TerminalApp.winmd", "TerminalApp.winmd"),
        (@"Microsoft.Terminal.Settings.Editor\Microsoft.Terminal.Settings.Editor.winmd", "Microsoft.Terminal.Settings.Editor.winmd"),
        (@"Microsoft.Terminal.UI.Markdown\Microsoft.Terminal.UI.Markdown.winmd", "Microsoft.Terminal.UI.Markdown.winmd"),
        (@"WindowsTerminal\Microsoft.UI.Xaml.dll", "Microsoft.UI.Xaml.dll"),
        (@"WindowsTerminal\WindowsTerminal.exe", "WindowsTerminal.exe"),
        ("wt.exe", "wtd.exe"),
        ("OpenConsole.exe", "OpenConsole.exe"),
        ("elevate-shim.exe", "elevate-shim.exe"),
        ("OpenConsoleProxy.dll", "OpenConsoleProxy.dll")
    };

    public static void Generate(string[] args)
    {
        var values = ParseArguments(args);
        var outputMap = Path.GetFullPath(GetRequired(values, "--output-map"));
        var repoRoot = Path.GetFullPath(GetRequired(values, "--repo-root"));
        var packageRoot = Path.GetFullPath(GetRequired(values, "--package-root"));
        var productBinRoot = Path.GetFullPath(GetRequired(values, "--product-bin-root"));
        var platform = GetRequired(values, "--platform");
        var manifest = RequireFile(GetRequired(values, "--manifest"));
        var resourcesPri = RequireFile(GetRequired(values, "--resources-pri"));

        var files = new List<(string Source, string Target)>
        {
            (manifest, "AppxManifest.xml"),
            (RequireFile(Path.Combine(repoRoot, "microsoft", "packages", "Microsoft.Web.WebView2.1.0.1661.34", "lib", "Microsoft.Web.WebView2.Core.winmd")), "Microsoft.Web.WebView2.Core.winmd"),
            (RequireFile(Path.Combine(packageRoot, "NOTICE.html")), "NOTICE.html")
        };

        AddDirectory(files, Path.Combine(repoRoot, "microsoft", "res", "terminal", "images-Dev"), "Images");
        AddDirectory(files, Path.Combine(repoRoot, "res", "web"), @"res\web");
        AddDirectory(files, Path.Combine(packageRoot, "ProfileIcons"), "ProfileIcons");
        AddDirectory(files, Path.Combine(packageRoot, "ProfileGeneratorIcons"), "ProfileGeneratorIcons");
        files.Add((RequireFile(Path.Combine(repoRoot, "microsoft", "src", "cascadia", "TerminalSettingsModel", "defaults.json")), "defaults.json"));

        foreach (var item in ProductPayload)
        {
            files.Add((RequireFile(Path.Combine(productBinRoot, item.Source)), item.Target));
        }

        files.Add((RequireFile(Path.Combine(repoRoot, "microsoft", "packages", "Microsoft.Internal.Windows.Terminal.ThemeHelpers.0.8.250811004", "runtimes", "win10-" + platform, "native", "TerminalThemeHelpers.dll")), "TerminalThemeHelpers.dll"));
        files.Add((RequireFile(Path.Combine(repoRoot, "microsoft", "packages", "Microsoft.Web.WebView2.1.0.1661.34", "runtimes", "win-" + platform, "native", "WebView2Loader.dll")), "WebView2Loader.dll"));
        files.Add((RequireFile(Path.Combine(repoRoot, "microsoft", "packages", "Microsoft.Web.WebView2.1.0.1661.34", "runtimes", "win-" + platform, "native_uap", "Microsoft.Web.WebView2.Core.dll")), "Microsoft.Web.WebView2.Core.dll"));
        files.Add((RequireFile(Path.Combine(repoRoot, "microsoft", "packages", "Microsoft.UI.Xaml.2.8.4", "lib", "uap10.0", "Microsoft.UI.Xaml.winmd")), "Microsoft.UI.Xaml.winmd"));
        files.Add((resourcesPri, "resources.pri"));
        files.Add((RequireFile(Path.Combine(repoRoot, "microsoft", "packages", "Microsoft.WindowsPackageManager.ComInterop.1.8.1911", "lib", "Microsoft.Management.Deployment.winmd")), "Microsoft.Management.Deployment.winmd"));

        if (files.Count != 256)
        {
            throw new InvalidOperationException($"Expected 256 package payload files, found {files.Count}.");
        }
        var duplicateTargets = files.GroupBy(item => item.Target, StringComparer.OrdinalIgnoreCase).Where(group => group.Count() != 1).ToArray();
        if (duplicateTargets.Length != 0)
        {
            throw new InvalidOperationException($"Package map has duplicate targets: {string.Join(", ", duplicateTargets.Select(group => group.Key))}.");
        }

        var outputDirectory = Path.GetDirectoryName(outputMap)
            ?? throw new InvalidOperationException($"Output map path {outputMap} has no directory.");
        Directory.CreateDirectory(outputDirectory);
        var lines = new List<string> { "[Files]" };
        lines.AddRange(files.Select(item => $"\"{item.Source}\" \"{item.Target}\""));
        File.WriteAllText(outputMap, string.Join("\r\n", lines) + "\r\n", new UTF8Encoding(false));
    }

    private static void AddDirectory(List<(string Source, string Target)> files, string sourceDirectory, string targetDirectory)
    {
        if (!Directory.Exists(sourceDirectory))
        {
            throw new InvalidOperationException($"Could not find package input directory {sourceDirectory}.");
        }
        foreach (var source in Directory.EnumerateFiles(sourceDirectory, "*", SearchOption.TopDirectoryOnly).OrderBy(path => path, StringComparer.OrdinalIgnoreCase))
        {
            files.Add((RequireFile(source), targetDirectory + "\\" + Path.GetFileName(source)));
        }
    }

    private static string RequireFile(string path)
    {
        var fullPath = Path.GetFullPath(path);
        return File.Exists(fullPath)
            ? fullPath
            : throw new InvalidOperationException($"Could not find required package input {fullPath}.");
    }

    private static Dictionary<string, string> ParseArguments(string[] args)
    {
        var values = new Dictionary<string, string>(StringComparer.Ordinal);
        for (var index = 0; index < args.Length; index += 2)
        {
            if (index + 1 >= args.Length || !args[index].StartsWith("--", StringComparison.Ordinal))
            {
                throw new ArgumentException("generate-package-map arguments must be explicit name/value pairs.");
            }
            values.Add(args[index], args[index + 1]);
        }
        return values;
    }

    private static string GetRequired(IReadOnlyDictionary<string, string> values, string key)
    {
        return values.TryGetValue(key, out var value) && !string.IsNullOrWhiteSpace(value)
            ? value
            : throw new ArgumentException($"Missing required argument {key}");
    }
}
