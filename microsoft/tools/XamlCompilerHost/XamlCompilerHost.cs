using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Reflection;
using Microsoft.Build.Framework;

internal sealed class TaskItem : ITaskItem
{
    private readonly Dictionary<string, string> _metadata = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);

    public TaskItem(string itemSpec) => ItemSpec = itemSpec;
    public string ItemSpec { get; set; }
    public int MetadataCount => _metadata.Count;
    public ICollection MetadataNames => _metadata.Keys;
    public IDictionary CloneCustomMetadata() => new Hashtable(_metadata, StringComparer.OrdinalIgnoreCase);
    public void CopyMetadataTo(ITaskItem destinationItem)
    {
        foreach (var pair in _metadata)
        {
            destinationItem.SetMetadata(pair.Key, pair.Value);
        }
    }
    public string GetMetadata(string metadataName) => _metadata.TryGetValue(metadataName, out var value) ? value : string.Empty;
    public void RemoveMetadata(string metadataName) => _metadata.Remove(metadataName);
    public void SetMetadata(string metadataName, string metadataValue) => _metadata[metadataName] = metadataValue;
}

internal sealed class BuildEngine : IBuildEngine
{
    public bool ContinueOnError => false;
    public int LineNumberOfTaskNode => 0;
    public int ColumnNumberOfTaskNode => 0;
    public string ProjectFileOfTaskNode => "CMakeLists.txt";

    public void LogErrorEvent(BuildErrorEventArgs e) => Console.Error.WriteLine(e.Message);
    public void LogWarningEvent(BuildWarningEventArgs e) => Console.Error.WriteLine(e.Message);
    public void LogMessageEvent(BuildMessageEventArgs e) => Console.WriteLine(e.Message);
    public void LogCustomEvent(CustomBuildEventArgs e) => Console.WriteLine(e.Message);
    public bool BuildProjectFile(string projectFileName, string[] targetNames, IDictionary globalProperties, IDictionary targetOutputs) => false;
}

internal static class Program
{
    private static string _frameworkRuntimeDirectory;
    private static string _taskDirectory;

    private static int Main(string[] args)
    {
        try
        {
            var options = Parse(args);
            _frameworkRuntimeDirectory = Required(options, "framework-runtime-dir");
            _taskDirectory = Path.GetDirectoryName(Required(options, "task-assembly"));
            AppDomain.CurrentDomain.AssemblyResolve += ResolveAssembly;
            return Run(options);
        }
        catch (Exception e)
        {
            for (var current = e; current != null; current = current.InnerException)
            {
                Console.Error.WriteLine(current.GetType().FullName + ": " + current.Message);
                var reflectionError = current as ReflectionTypeLoadException;
                if (reflectionError != null)
                {
                    foreach (var loaderError in reflectionError.LoaderExceptions)
                    {
                        Console.Error.WriteLine(loaderError.GetType().FullName + ": " + loaderError.Message);
                    }
                }
            }
            return 1;
        }
    }

    private static Assembly ResolveAssembly(object sender, ResolveEventArgs args)
    {
        var fileName = new AssemblyName(args.Name).Name + ".dll";
        foreach (var directory in new[] { _frameworkRuntimeDirectory, _taskDirectory })
        {
            var path = Path.Combine(directory, fileName);
            if (File.Exists(path))
            {
                return Assembly.LoadFrom(path);
            }
        }
        return null;
    }

    private static int Run(Dictionary<string, List<string>> options)
    {
        var sourceItems = Values(options, "page");
        if (sourceItems.Count == 0)
        {
            sourceItems = Values(options, "application-definition");
        }
        Directory.SetCurrentDirectory(Path.GetDirectoryName(Path.GetFullPath(sourceItems[0])));
        var taskAssembly = Assembly.LoadFrom(Required(options, "task-assembly"));
        var taskType = taskAssembly.GetType("Microsoft.Windows.UI.Xaml.Build.Tasks.CompileXaml", true);
        var task = Activator.CreateInstance(taskType);
        var outputPath = EnsureTrailingSeparator(Required(options, "output"));
        var windowsSdkPath = EnsureTrailingSeparator(Required(options, "windows-sdk-path"));
        Set(taskType, task, "BuildEngine", new BuildEngine());
        Set(taskType, task, "OutputPath", outputPath);
        Set(taskType, task, "OutputType", Required(options, "output-type"));
        Set(taskType, task, "Language", Required(options, "language"));
        var compileMode = Required(options, "compile-mode");
        var isPass1 = string.Equals(compileMode, "RealBuildPass1", StringComparison.Ordinal);
        Set(taskType, task, "CompileMode", compileMode);
        Set(taskType, task, "RootNamespace", Required(options, "root-namespace"));
        Set(taskType, task, "XamlComponentResourceLocation", "nested");
        var codeGenerationControlFlags = Values(options, "code-generation-control-flags");
        if (codeGenerationControlFlags.Count != 0)
        {
            Set(taskType, task, "CodeGenerationControlFlags", codeGenerationControlFlags[0]);
        }
        Set(taskType, task, "WindowsSdkPath", windowsSdkPath);
        Set(taskType, task, "SavedStateFile", Path.Combine(outputPath, "XamlSaveStateFile.xml"));
        Set(taskType, task, "VCInstallDir", EnsureTrailingSeparator(Required(options, "vc-install-dir")));
        Set(taskType, task, "LanguageSourceExtension", ".cpp");
        Set(taskType, task, "IsPass1", isPass1);
        Set(taskType, task, "XAMLFingerprint", true);
        Set(taskType, task, "EnableTypeInfoReflection", false);
        Set(taskType, task, "TargetPlatformMinVersion", Required(options, "target-platform-min-version"));
        Set(taskType, task, "ProjectPath", Required(options, "project-path"));
        Set(taskType, task, "ProjectName", Required(options, "project-name"));
        Set(taskType, task, "FingerprintIgnorePaths", new[] { windowsSdkPath });
        Set(taskType, task, "EnableDefaultValidationContextGeneration", true);
        Set(taskType, task, "PriIndexName", Required(options, "pri-index-name"));
        Set(taskType, task, "CIncludeDirectories", string.Join(";", Values(options, "include")));
        if (!isPass1)
        {
            Set(taskType, task, "DisableXbfLineInfo", true);
            Set(taskType, task, "DisableXbfGeneration", false);
            Set(taskType, task, "PlatformXmlDir", EnsureTrailingSeparator(Required(options, "platform-xml-dir")));
            Set(taskType, task, "LocalAssembly", Items(options, "local-assembly"));
            Set(taskType, task, "ClIncludeFiles", ClIncludeItems(options));
        }

        var pages = new List<ITaskItem>();
        foreach (var path in Values(options, "page"))
        {
            var item = new TaskItem(path);
            item.SetMetadata("GeneratorTarget", "DesignTimeMarkupCompilation");
            item.SetMetadata("SubType", "Designer");
            pages.Add(item);
        }
        Set(taskType, task, "XamlPages", pages.ToArray());

        var applications = new List<ITaskItem>();
        foreach (var path in Values(options, "application-definition"))
        {
            var item = new TaskItem(path);
            item.SetMetadata("GeneratorTarget", "DesignTimeMarkupCompilation");
            item.SetMetadata("SubType", "Designer");
            applications.Add(item);
        }
        Set(taskType, task, "XamlApplications", applications.ToArray());

        var references = new List<ITaskItem>();
        foreach (var path in Values(options, "reference"))
        {
            references.Add(new TaskItem(path));
        }
        foreach (var path in Values(options, "winmd-reference"))
        {
            references.Add(WinmdItem(path, false));
        }
        foreach (var path in Values(options, "system-reference"))
        {
            references.Add(WinmdItem(path, true));
        }
        Set(taskType, task, "ReferenceAssemblies", references.ToArray());

        Directory.CreateDirectory(outputPath);
        if (!(bool)taskType.GetMethod("Execute").Invoke(task, null))
        {
            return 1;
        }
        foreach (ITaskItem item in (IEnumerable)taskType.GetProperty("GeneratedCodeFiles").GetValue(task, null))
        {
            Console.WriteLine("CODE=" + item.ItemSpec);
        }
        foreach (ITaskItem item in (IEnumerable)taskType.GetProperty("GeneratedXamlFiles").GetValue(task, null))
        {
            Console.WriteLine("XAML=" + item.ItemSpec);
        }
        foreach (ITaskItem item in (IEnumerable)taskType.GetProperty("GeneratedXbfFiles").GetValue(task, null))
        {
            Console.WriteLine("XBF=" + item.ItemSpec);
        }
        return 0;
    }

    private static void Set(Type taskType, object task, string name, object value)
    {
        taskType.GetProperty(name).SetValue(task, value, null);
    }

    private static ITaskItem[] Items(Dictionary<string, List<string>> options, string name)
    {
        var items = new List<ITaskItem>();
        foreach (var value in Values(options, name))
        {
            items.Add(new TaskItem(value));
        }
        return items.ToArray();
    }

    private static ITaskItem[] ClIncludeItems(Dictionary<string, List<string>> options)
    {
        var items = new List<ITaskItem>();
        foreach (var value in Values(options, "cl-include"))
        {
            var fields = value.Split(new[] { '#' }, 3);
            var item = FileItem(fields.Length == 3 ? fields[2] : fields[0]);
            if (fields.Length >= 2 && fields[1].Length != 0)
            {
                item.SetMetadata("DependentUpon", fields[1]);
            }
            items.Add(item);
        }
        return items.ToArray();
    }

    private static TaskItem FileItem(string path)
    {
        var absolutePath = Path.GetFullPath(path);
        var item = new TaskItem(absolutePath);
        item.SetMetadata("FullPath", absolutePath);
        item.SetMetadata("RootDir", Path.GetPathRoot(absolutePath));
        item.SetMetadata("Filename", Path.GetFileNameWithoutExtension(absolutePath));
        item.SetMetadata("Extension", Path.GetExtension(absolutePath));
        item.SetMetadata("RelativeDir", Path.GetDirectoryName(absolutePath) + Path.DirectorySeparatorChar);
        item.SetMetadata("Directory", Path.GetDirectoryName(absolutePath) + Path.DirectorySeparatorChar);
        return item;
    }

    private static TaskItem WinmdItem(string path, bool systemReference)
    {
        var item = new TaskItem(path);
        item.SetMetadata("IsWinMDFile", "true");
        item.SetMetadata("WinMDFile", "true");
        item.SetMetadata("WinMDFileType", "Native");
        item.SetMetadata("ReferenceOutputAssembly", "true");
        item.SetMetadata("FusionName", AssemblyName.GetAssemblyName(path).FullName);
        if (systemReference)
        {
            item.SetMetadata("IsSystemReference", "True");
            item.SetMetadata("FrameworkFile", "true");
        }
        return item;
    }

    private static Dictionary<string, List<string>> Parse(string[] args)
    {
        var result = new Dictionary<string, List<string>>(StringComparer.OrdinalIgnoreCase);
        for (var i = 0; i < args.Length; i += 2)
        {
            if (args[i] == "||")
            {
                break;
            }
            if (!args[i].StartsWith("--", StringComparison.Ordinal) || i + 1 >= args.Length)
            {
                throw new ArgumentException("Arguments must be --name value pairs; index=" + i + ", value=" + args[i] + ", count=" + args.Length + ".");
            }
            var name = args[i].Substring(2);
            if (!result.TryGetValue(name, out var values))
            {
                values = new List<string>();
                result.Add(name, values);
            }
            values.Add(args[i + 1]);
        }
        return result;
    }

    private static string Required(Dictionary<string, List<string>> options, string name)
    {
        var values = Values(options, name);
        if (values.Count != 1)
        {
            throw new ArgumentException("Expected exactly one --" + name + ".");
        }
        return values[0];
    }

    private static List<string> Values(Dictionary<string, List<string>> options, string name)
    {
        return options.TryGetValue(name, out var values) ? values : new List<string>();
    }

    private static string EnsureTrailingSeparator(string path)
    {
        return path.EndsWith(Path.DirectorySeparatorChar.ToString(), StringComparison.Ordinal) ? path : path + Path.DirectorySeparatorChar;
    }
}
