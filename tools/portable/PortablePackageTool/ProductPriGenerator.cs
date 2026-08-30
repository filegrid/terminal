using System.Text;
using System.Xml;
using System.Xml.Linq;

internal static class ProductPriGenerator
{
    private static readonly (string Directory, string File)[] ProductPriFiles =
    {
        ("Microsoft.Terminal.Control.Lib", "Microsoft.Terminal.Control.pri"),
        ("Microsoft.Terminal.Settings.Editor", "Microsoft.Terminal.Settings.Editor.pri"),
        ("Microsoft.Terminal.Settings.Model.Lib", "Microsoft.Terminal.Settings.Model.pri"),
        ("Microsoft.Terminal.UI.Markdown", "Microsoft.Terminal.UI.Markdown.pri"),
        ("Microsoft.Terminal.UI", "Microsoft.Terminal.UI.pri"),
        ("TerminalApp", "TerminalApp.pri"),
        ("TerminalAppLib", "TerminalApp.pri"),
        ("TerminalConnection", "Microsoft.Terminal.TerminalConnection.pri"),
        ("TerminalCore", "Microsoft.Terminal.Core.pri")
    };

    public static void Generate(string[] args)
    {
        var values = ParseArguments(args);
        var outputRoot = Path.GetFullPath(GetRequired(values, "--output-root"));
        var repoRoot = Path.GetFullPath(GetRequired(values, "--repo-root"));
        var packageRoot = Path.GetFullPath(GetRequired(values, "--package-root"));
        var productBinRoot = Path.GetFullPath(GetRequired(values, "--product-bin-root"));
        var xamlPri = RequireFile(GetRequired(values, "--xaml-pri"));

        Directory.CreateDirectory(outputRoot);
        var embedRoot = Path.Combine(outputRoot, "embed");
        Directory.CreateDirectory(embedRoot);

        var layoutFiles = BuildLayoutFiles(repoRoot, packageRoot);
        RequireCount(layoutFiles, 226, "package layout resources");

        var resourceFiles = Directory
            .EnumerateFiles(Path.Combine(packageRoot, "Resources"), "*.resw", SearchOption.AllDirectories)
            .Select(RequireFile)
            .OrderBy(path => path, StringComparer.OrdinalIgnoreCase)
            .ToArray();
        RequireCount(resourceFiles, 90, "package RESW resources");

        var priFiles = ProductPriFiles
            .Select(item => RequireFile(Path.Combine(productBinRoot, item.Directory, item.File)))
            .Append(xamlPri)
            .ToArray();
        RequireCount(priFiles, ProductPriFiles.Length + 1, "merged PRI resources");

        var layoutList = Path.Combine(outputRoot, "filtered.layout.resfiles");
        var resourcesList = Path.Combine(outputRoot, "resources.resfiles");
        var priList = Path.Combine(outputRoot, "pri.resfiles");
        var embedList = Path.Combine(embedRoot, "embed.resfiles");
        WriteLines(layoutList, layoutFiles);
        WriteLines(resourcesList, resourceFiles);
        WriteLines(priList, priFiles);
        File.WriteAllText(embedList, string.Empty, new UTF8Encoding(false));

        WriteConfig(
            Path.Combine(outputRoot, "priconfig.xml"),
            RelativeWindowsPath(repoRoot, layoutList),
            RelativeWindowsPath(repoRoot, resourcesList),
            RelativeWindowsPath(repoRoot, priList),
            RelativeWindowsPath(repoRoot, embedRoot),
            "embed.resfiles");
    }

    private static string[] BuildLayoutFiles(string repoRoot, string packageRoot)
    {
        var imagesRoot = Path.Combine(repoRoot, "microsoft", "res", "terminal", "images-Dev");
        var items = Directory
            .EnumerateFiles(imagesRoot, "*", SearchOption.TopDirectoryOnly)
            .Select(path => "Images\\" + Path.GetFileName(RequireFile(path)))
            .OrderBy(path => path, StringComparer.OrdinalIgnoreCase)
            .ToList();

        RequireCount(items, 192, "Dev package images");
        items.Add("NOTICE.html");
        items.AddRange(EnumerateLayoutDirectory(packageRoot, "ProfileGeneratorIcons"));
        items.AddRange(EnumerateLayoutDirectory(packageRoot, "ProfileIcons"));
        items.Add("defaults.json");

        RequireFile(Path.Combine(packageRoot, "NOTICE.html"));
        RequireFile(Path.Combine(repoRoot, "microsoft", "src", "cascadia", "TerminalSettingsModel", "defaults.json"));
        return items.ToArray();
    }

    private static IEnumerable<string> EnumerateLayoutDirectory(string packageRoot, string directory)
    {
        return Directory
            .EnumerateFiles(Path.Combine(packageRoot, directory), "*", SearchOption.TopDirectoryOnly)
            .Select(path => directory + "\\" + Path.GetFileName(RequireFile(path)))
            .OrderBy(path => path, StringComparer.OrdinalIgnoreCase);
    }

    private static void WriteConfig(
        string outputPath,
        string layoutList,
        string resourcesList,
        string priList,
        string embedRoot,
        string embedList)
    {
        var resources = new XElement("resources",
            new XAttribute("targetOsVersion", "10.0.0"),
            new XAttribute("majorVersion", "1"),
            CreateIndex("\\", layoutList, "RESFILES"),
            CreateIndex("\\", resourcesList, "RESW", "RESJSON", "RESFILES"),
            CreateIndex("\\", priList, "PRI", "RESFILES"),
            CreateIndex(embedRoot, embedList, "RESFILES", "EMBEDFILES"));

        var settings = new XmlWriterSettings
        {
            Encoding = new UTF8Encoding(false),
            Indent = true,
            OmitXmlDeclaration = false,
            NewLineChars = "\r\n"
        };
        using var writer = XmlWriter.Create(outputPath, settings);
        new XDocument(resources).Save(writer);
    }

    private static XElement CreateIndex(string root, string startIndexAt, params string[] indexers)
    {
        var index = new XElement("index",
            new XAttribute("root", root),
            new XAttribute("startIndexAt", startIndexAt),
            new XElement("default",
                Qualifier("Language", "en-US"),
                Qualifier("Contrast", "standard"),
                Qualifier("Scale", "200"),
                Qualifier("HomeRegion", "001"),
                Qualifier("TargetSize", "256"),
                Qualifier("LayoutDirection", "LTR"),
                Qualifier("DXFeatureLevel", "DX9"),
                Qualifier("Configuration", string.Empty),
                Qualifier("AlternateForm", string.Empty),
                Qualifier("Platform", "UAP")));

        foreach (var indexer in indexers)
        {
            var config = new XElement("indexer-config", new XAttribute("type", indexer));
            if (indexer == "RESW")
            {
                config.SetAttributeValue("convertDotsToSlashes", "true");
            }
            else if (indexer == "RESFILES")
            {
                config.SetAttributeValue("qualifierDelimiter", ".");
            }
            index.Add(config);
        }
        return index;
    }

    private static XElement Qualifier(string name, string value) =>
        new("qualifier", new XAttribute("name", name), new XAttribute("value", value));

    private static void WriteLines(string path, IEnumerable<string> lines)
    {
        File.WriteAllText(path, string.Join("\r\n", lines) + "\r\n", new UTF8Encoding(false));
    }

    private static string RelativeWindowsPath(string root, string path) =>
        Path.GetRelativePath(root, path).Replace('/', '\\');

    private static string RequireFile(string path)
    {
        var fullPath = Path.GetFullPath(path);
        return File.Exists(fullPath)
            ? fullPath
            : throw new InvalidOperationException($"Could not find required PRI input {fullPath}.");
    }

    private static void RequireCount<T>(IReadOnlyCollection<T> values, int expected, string description)
    {
        if (values.Count != expected)
        {
            throw new InvalidOperationException($"Expected {expected} {description}, found {values.Count}.");
        }
    }

    private static Dictionary<string, string> ParseArguments(string[] args)
    {
        var values = new Dictionary<string, string>(StringComparer.Ordinal);
        for (var index = 0; index < args.Length; index += 2)
        {
            if (index + 1 >= args.Length || !args[index].StartsWith("--", StringComparison.Ordinal))
            {
                throw new ArgumentException("generate-pri-config arguments must be explicit name/value pairs.");
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
