using System.Reflection;
using System.Reflection.Metadata;
using System.Reflection.PortableExecutable;
using System.Text;
using System.Xml;
using System.Xml.Linq;

internal static class ProductManifestGenerator
{
    private const string MetadataNamespace = "Windows.Foundation.Metadata.";

    private static readonly HashSet<string> FactoryAttributes = new(StringComparer.Ordinal)
    {
        MetadataNamespace + "ActivatableAttribute",
        MetadataNamespace + "ComposableAttribute",
        MetadataNamespace + "StaticAttribute"
    };

    private static readonly (string DllName, string OutputDirectory, string WinmdName)[] Registrations =
    {
        ("Microsoft.Terminal.Control.dll", "Microsoft.Terminal.Control", "Microsoft.Terminal.Control.winmd"),
        ("Microsoft.Terminal.Settings.Editor.dll", "Microsoft.Terminal.Settings.Editor", "Microsoft.Terminal.Settings.Editor.winmd"),
        ("Microsoft.Terminal.Settings.Model.dll", "Microsoft.Terminal.Settings.Model", "Microsoft.Terminal.Settings.Model.winmd"),
        ("TerminalConnection.dll", "TerminalConnection", "Microsoft.Terminal.TerminalConnection.winmd"),
        ("Microsoft.Terminal.UI.Markdown.dll", "Microsoft.Terminal.UI.Markdown", "Microsoft.Terminal.UI.Markdown.winmd"),
        ("Microsoft.Terminal.UI.dll", "Microsoft.Terminal.UI", "Microsoft.Terminal.UI.winmd"),
        ("TerminalApp.dll", "TerminalApp", "TerminalApp.winmd")
    };

    public static void Generate(string[] args)
    {
        var values = ParseArguments(args);
        var sourceManifest = GetRequired(values, "--source-manifest");
        var outputManifest = GetRequired(values, "--output-manifest");
        var architecture = GetRequired(values, "--architecture");
        var productBinRoot = GetRequired(values, "--product-bin-root");

        var document = XDocument.Load(sourceManifest, LoadOptions.PreserveWhitespace);
        var package = document.Root ?? throw new InvalidOperationException($"Manifest {sourceManifest} has no Package root.");
        var foundation = package.Name.Namespace;

        var identity = package.Element(foundation + "Identity")
            ?? throw new InvalidOperationException($"Manifest {sourceManifest} has no Identity element.");
        identity.SetAttributeValue("ProcessorArchitecture", architecture);

        var application = package
            .Element(foundation + "Applications")?
            .Element(foundation + "Application")
            ?? throw new InvalidOperationException($"Manifest {sourceManifest} has no Application element.");
        application.SetAttributeValue("Executable", "WindowsTerminal.exe");
        application.SetAttributeValue("EntryPoint", "Windows.FullTrustApplication");

        var dependencies = package.Element(foundation + "Dependencies")
            ?? throw new InvalidOperationException($"Manifest {sourceManifest} has no Dependencies element.");
        dependencies.Add(new XElement(
            foundation + "PackageDependency",
            new XAttribute("Name", "Microsoft.UI.Xaml.2.8"),
            new XAttribute("MinVersion", "8.2305.5001.0"),
            new XAttribute("Publisher", "CN=Microsoft Corporation, O=Microsoft Corporation, L=Redmond, S=Washington, C=US")));

        if (package.Elements(foundation + "Extensions").Any())
        {
            throw new InvalidOperationException($"Manifest {sourceManifest} already contains package-level Extensions.");
        }

        var extensions = new XElement(foundation + "Extensions");
        foreach (var registration in Registrations)
        {
            var outputDirectory = Path.Combine(productBinRoot, registration.OutputDirectory);
            var dllPath = Path.Combine(outputDirectory, registration.DllName);
            var winmdPath = Path.Combine(outputDirectory, registration.WinmdName);
            if (!File.Exists(dllPath))
            {
                throw new InvalidOperationException($"Could not find product DLL {dllPath}.");
            }
            if (!File.Exists(winmdPath))
            {
                throw new InvalidOperationException($"Could not find product WinMD {winmdPath}.");
            }

            var inProcessServer = new XElement(
                foundation + "InProcessServer",
                new XElement(foundation + "Path", registration.DllName));
            foreach (var className in ReadActivatableClasses(winmdPath))
            {
                inProcessServer.Add(new XElement(
                    foundation + "ActivatableClass",
                    new XAttribute("ActivatableClassId", className),
                    new XAttribute("ThreadingModel", "both")));
            }

            extensions.Add(new XElement(
                foundation + "Extension",
                new XAttribute("Category", "windows.activatableClass.inProcessServer"),
                inProcessServer));
        }
        package.Add(extensions);

        var outputDirectoryPath = Path.GetDirectoryName(outputManifest);
        if (string.IsNullOrWhiteSpace(outputDirectoryPath))
        {
            throw new InvalidOperationException($"Output manifest path {outputManifest} has no directory.");
        }
        Directory.CreateDirectory(outputDirectoryPath);

        var settings = new XmlWriterSettings
        {
            Encoding = new UTF8Encoding(false),
            Indent = true,
            OmitXmlDeclaration = false,
            NewLineChars = "\r\n"
        };
        using var writer = XmlWriter.Create(outputManifest, settings);
        document.Save(writer);
    }

    private static IReadOnlyList<string> ReadActivatableClasses(string winmdPath)
    {
        using var stream = File.OpenRead(winmdPath);
        using var peReader = new PEReader(stream);
        var reader = peReader.GetMetadataReader();
        var result = new List<string>();

        foreach (var handle in reader.TypeDefinitions)
        {
            var type = reader.GetTypeDefinition(handle);
            var attributes = new HashSet<string>(StringComparer.Ordinal);
            CustomAttribute? threadingAttribute = null;

            foreach (var attributeHandle in type.GetCustomAttributes())
            {
                var attribute = reader.GetCustomAttribute(attributeHandle);
                var attributeName = GetAttributeTypeName(reader, attribute);
                attributes.Add(attributeName);
                if (string.Equals(attributeName, MetadataNamespace + "ThreadingAttribute", StringComparison.Ordinal))
                {
                    threadingAttribute = attribute;
                }
            }

            if (!attributes.Overlaps(FactoryAttributes))
            {
                continue;
            }
            if (threadingAttribute is null)
            {
                throw new InvalidOperationException($"Runtime class {GetTypeName(reader, type)} has no ThreadingAttribute.");
            }

            var value = reader.GetBlobReader(threadingAttribute.Value.Value);
            if (value.ReadUInt16() != 1 || value.ReadInt32() != 3)
            {
                throw new InvalidOperationException($"Runtime class {GetTypeName(reader, type)} does not use the required both threading model.");
            }

            result.Add(GetTypeName(reader, type));
        }

        return result;
    }

    private static string GetAttributeTypeName(MetadataReader reader, CustomAttribute attribute)
    {
        EntityHandle typeHandle;
        switch (attribute.Constructor.Kind)
        {
            case HandleKind.MemberReference:
                typeHandle = reader.GetMemberReference((MemberReferenceHandle)attribute.Constructor).Parent;
                break;
            case HandleKind.MethodDefinition:
                typeHandle = reader.GetMethodDefinition((MethodDefinitionHandle)attribute.Constructor).GetDeclaringType();
                break;
            default:
                throw new InvalidOperationException($"Unsupported custom attribute constructor {attribute.Constructor.Kind}.");
        }

        return typeHandle.Kind switch
        {
            HandleKind.TypeReference => GetTypeName(reader, reader.GetTypeReference((TypeReferenceHandle)typeHandle)),
            HandleKind.TypeDefinition => GetTypeName(reader, reader.GetTypeDefinition((TypeDefinitionHandle)typeHandle)),
            _ => throw new InvalidOperationException($"Unsupported custom attribute type {typeHandle.Kind}.")
        };
    }

    private static string GetTypeName(MetadataReader reader, TypeDefinition type)
    {
        return JoinTypeName(reader.GetString(type.Namespace), reader.GetString(type.Name));
    }

    private static string GetTypeName(MetadataReader reader, TypeReference type)
    {
        return JoinTypeName(reader.GetString(type.Namespace), reader.GetString(type.Name));
    }

    private static string JoinTypeName(string typeNamespace, string name)
    {
        return string.IsNullOrEmpty(typeNamespace) ? name : typeNamespace + "." + name;
    }

    private static Dictionary<string, string> ParseArguments(string[] args)
    {
        var values = new Dictionary<string, string>(StringComparer.Ordinal);
        for (var index = 0; index < args.Length; index += 2)
        {
            if (index + 1 >= args.Length || !args[index].StartsWith("--", StringComparison.Ordinal))
            {
                throw new ArgumentException("generate-manifest arguments must be explicit name/value pairs.");
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
