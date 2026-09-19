using System.Text;
using System.Text.RegularExpressions;

namespace ArchitectureChecks;

public sealed class SpaceGameLayerChecker
{
    private const string simulation_module = "SpaceGameSimulation";
    private const string presentation_module = "SpaceGamePresentation";
    private const string composition_module = "SpaceGame";

    private static readonly HashSet<string> allowed_dependencies = new(StringComparer.Ordinal)
    {
        "Core",
        "CoreUObject",
        "Engine",
        "TraceLog",
        "NativeMemory",
        "NativeSimulation",
        "SandboxCore",
        "SandboxCoreEngine",
    };

    private static readonly Regex forbidden_simulation_reference = new(
        """
        presentation|SpaceGameRendering|Niagara|SandboxISMC|SandboxUI|
        InstancedStaticMeshComponent|UserWidget|DrawDebug|
        \b(?:UMG|Slate|SlateCore|CommonUI)\b|ATestBatchOrchestrator|
        [#]\s*include\s*[<\"](?:SpaceGame|SandboxGameShared)/
        """.ReplaceLineEndings(string.Empty),
        RegexOptions.Compiled | RegexOptions.CultureInvariant | RegexOptions.IgnoreCase);

    private static readonly Regex mutable_simulation_access = new(
        @"\b(?:FLevelSimulation|Simulation)\s*[*&]|\bconst_cast\s*<",
        RegexOptions.Compiled | RegexOptions.CultureInvariant);

    private static readonly string[] source_extensions = [".h", ".cpp", ".inl"];

    private readonly BuildCsDependencyParser dependency_parser;

    public SpaceGameLayerChecker(BuildCsDependencyParser? dependency_parser = null)
    {
        this.dependency_parser = dependency_parser ?? new BuildCsDependencyParser();
    }

    public ArchitectureCheckResult Check(ArchitectureCheckRequest request)
    {
        ArgumentNullException.ThrowIfNull(request);
        ArgumentException.ThrowIfNullOrWhiteSpace(request.RootPath);

        var root = Path.GetFullPath(request.RootPath);
        if (!Directory.Exists(root))
        {
            throw new DirectoryNotFoundException($"Repository root does not exist: '{root}'.");
        }

        var diagnostics = new List<string>();
        var modules = DiscoverModules(root);
        CheckSimulationDependencies(modules, diagnostics);

        var source = Path.Combine(root, "Plugins", "SpaceGame", "Source");
        CheckForbiddenSimulationReferences(root, Path.Combine(source, simulation_module), diagnostics);
        CheckPresentationSimulationAccess(root, Path.Combine(source, "SpaceGamePresentation"), diagnostics);
        CheckModuleComposition(modules, diagnostics);

        diagnostics.Sort(StringComparer.Ordinal);
        return new ArchitectureCheckResult(diagnostics);
    }

    private Dictionary<string, string> DiscoverModules(string root)
    {
        var modules = new Dictionary<string, string>(StringComparer.Ordinal);
        foreach (var directory_name in new[] { "Plugins", "Source" })
        {
            var directory = Path.Combine(root, directory_name);
            if (!Directory.Exists(directory))
            {
                continue;
            }

            foreach (var path in Directory.EnumerateFiles(directory, "*.Build.cs", SearchOption.AllDirectories)
                .OrderBy(path => path, StringComparer.Ordinal))
            {
                var file_name = Path.GetFileName(path);
                var module_name = file_name[..^".Build.cs".Length];
                modules[module_name] = path;
            }
        }

        return modules;
    }

    private void CheckSimulationDependencies(
        IReadOnlyDictionary<string, string> modules,
        ICollection<string> diagnostics)
    {
        if (!modules.ContainsKey(simulation_module))
        {
            diagnostics.Add("SpaceGameSimulation runtime module is missing");
        }

        var pending = new Stack<string>();
        pending.Push(simulation_module);
        var visited = new HashSet<string>(StringComparer.Ordinal);
        while (pending.TryPop(out var module))
        {
            if (!visited.Add(module) || !modules.TryGetValue(module, out var path))
            {
                continue;
            }

            foreach (var dependency in dependency_parser.Parse(ReadText(path)).OrderBy(name => name, StringComparer.Ordinal))
            {
                if (!allowed_dependencies.Contains(dependency))
                {
                    diagnostics.Add($"{module} has forbidden simulation dependency {dependency}");
                }

                pending.Push(dependency);
            }
        }
    }

    private static void CheckForbiddenSimulationReferences(string root, string simulation_source, ICollection<string> diagnostics)
    {
        foreach (var path in EnumerateSourceFiles(simulation_source))
        {
            var lines = File.ReadAllLines(path, Encoding.UTF8);
            for (var index = 0; index < lines.Length; ++index)
            {
                if (forbidden_simulation_reference.IsMatch(lines[index]))
                {
                    diagnostics.Add($"{Path.GetRelativePath(root, path)}:{index + 1}: forbidden layer reference");
                }
            }
        }
    }

    private static void CheckPresentationSimulationAccess(string root, string presentation_source, ICollection<string> diagnostics)
    {
        foreach (var path in EnumerateSourceFiles(presentation_source))
        {
            if (mutable_simulation_access.IsMatch(ReadText(path)))
            {
                diagnostics.Add($"{Path.GetRelativePath(root, path)}: mutable simulation access");
            }
        }
    }

    private void CheckModuleComposition(IReadOnlyDictionary<string, string> modules, ICollection<string> diagnostics)
    {
        if (!modules.TryGetValue(presentation_module, out var presentation_path))
        {
            diagnostics.Add("SpaceGamePresentation runtime module is missing");
        }
        else
        {
            var dependencies = dependency_parser.Parse(ReadText(presentation_path));
            if (dependencies.Contains(composition_module) || !dependencies.Contains(simulation_module))
            {
                diagnostics.Add("Presentation must depend on Simulation, never on SpaceGame");
            }
        }

        if (!modules.TryGetValue(composition_module, out var composition_path))
        {
            diagnostics.Add("SpaceGame composition module is missing");
        }
        else
        {
            var dependencies = dependency_parser.Parse(ReadText(composition_path));
            if (!dependencies.Contains(simulation_module) || !dependencies.Contains(presentation_module))
            {
                diagnostics.Add("SpaceGame must compose both layers");
            }
        }
    }

    private static IEnumerable<string> EnumerateSourceFiles(string directory)
    {
        if (!Directory.Exists(directory))
        {
            return [];
        }

        return Directory.EnumerateFiles(directory, "*", SearchOption.AllDirectories)
            .Where(path => source_extensions.Contains(Path.GetExtension(path), StringComparer.Ordinal))
            .OrderBy(path => path, StringComparer.Ordinal);
    }

    private static string ReadText(string path)
    {
        return File.ReadAllText(path, Encoding.UTF8);
    }
}
