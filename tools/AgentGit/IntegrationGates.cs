using System.Text;
using System.Text.Json;
using System.Text.Json.Serialization;

namespace AgentGit;

internal enum IntegrationGate
{
    AgentGitTests,
    JobserverTests,
    CSharpToolsTests,
    ToolTests,
    PowerShellChecks,
    PythonChecks,
    CMakeChecks,
    NativeTests,
    CodegenTests,
    UnrealTests,
    DevelopmentBuild,
    BenchmarkBuild,
}

internal sealed record IntegrationGatePlan(
    IReadOnlyList<string> Components,
    IReadOnlyList<IntegrationGate> Gates,
    IReadOnlyDictionary<IntegrationGate, string> Reasons)
{
    public bool RequiresUnreal => Gates.Contains(IntegrationGate.UnrealTests) ||
        Gates.Contains(IntegrationGate.DevelopmentBuild);
}

internal sealed class IntegrationGatePlanner
{
    private static readonly IntegrationGate[] global_gates =
    [
        IntegrationGate.AgentGitTests,
        IntegrationGate.JobserverTests,
        IntegrationGate.PowerShellChecks,
        IntegrationGate.PythonChecks,
        IntegrationGate.CMakeChecks,
        IntegrationGate.CSharpToolsTests,
        IntegrationGate.NativeTests,
        IntegrationGate.CodegenTests,
        IntegrationGate.UnrealTests,
        IntegrationGate.DevelopmentBuild,
    ];

    private static readonly IntegrationGate[] build_global_gates =
    [
        IntegrationGate.CMakeChecks,
        IntegrationGate.CSharpToolsTests,
        IntegrationGate.NativeTests,
        IntegrationGate.CodegenTests,
        IntegrationGate.UnrealTests,
        IntegrationGate.DevelopmentBuild,
    ];

    private readonly IReadOnlyList<ComponentRule> rules;

    public IntegrationGatePlanner() : this(DefaultRules())
    {
    }

    internal IntegrationGatePlanner(IReadOnlyList<ComponentRule> rules)
    {
        this.rules = rules;
    }

    public static async Task<IntegrationGatePlanner> LoadAsync(
        GitClient git,
        string worktree,
        string policy_commit,
        CancellationToken cancellation_token)
    {
        var result = await git.RunAsync(
            worktree,
            ["show", $"{policy_commit}:.integration-gates.json"],
            cancellation_token: cancellation_token);
        if (result.ExitCode != 0)
        {
            return new IntegrationGatePlanner();
        }

        return new IntegrationGatePlanner(ParseManifest(Encoding.UTF8.GetString(result.StandardOutput)));
    }

    public IntegrationGatePlan Plan(IReadOnlyList<string> changed_paths, bool include_tool_tests = false)
    {
        var components = new SortedSet<string>(StringComparer.Ordinal);
        var gates = new HashSet<IntegrationGate>();
        var reasons = new Dictionary<IntegrationGate, string>();
        foreach (var raw_path in changed_paths)
        {
            var path = raw_path.Replace('\\', '/');
            foreach (var gate in MinimumGates(path))
            {
                gates.Add(gate);
                reasons.TryAdd(gate, $"built-in safety classification for '{path}'");
            }
            var matched = rules
                .Select(rule => (Rule: rule, Length: rule.MatchLength(path)))
                .Where(value => value.Length >= 0)
                .ToArray();
            var longest = matched.Length == 0 ? -1 : matched.Max(value => value.Length);
            var matches = matched.Where(value => value.Length == longest).Select(value => value.Rule).ToArray();
            if (matches.Length == 0)
            {
                Add("unclassified", global_gates, $"'{path}' is not owned by a known component");
                continue;
            }

            var pending = new Queue<(ComponentRule Rule, string Reason)>(matches.Select(match =>
                (match, $"'{path}' belongs to {match.Name}")));
            var expanded = new HashSet<string>(StringComparer.Ordinal);
            while (pending.TryDequeue(out var current))
            {
                if (!expanded.Add(current.Rule.Name))
                {
                    continue;
                }

                Add(current.Rule.Name, current.Rule.Gates, current.Reason);
                foreach (var affected_name in current.Rule.Affects)
                {
                    var affected = rules.SingleOrDefault(rule => rule.Name == affected_name)
                        ?? throw new PolicyConfigurationException(
                            $"Integration gate component '{current.Rule.Name}' affects unknown component '{affected_name}'.");
                    pending.Enqueue((affected, $"{affected.Name} depends on {current.Rule.Name}"));
                }
            }
        }

        if (include_tool_tests)
        {
            Add("explicit-tool-tests", [IntegrationGate.ToolTests], "requested with --tool-tests");
        }

        return new IntegrationGatePlan(
            components.ToArray(),
            gates.OrderBy(value => value).ToArray(),
            reasons);

        void Add(string component, IEnumerable<IntegrationGate> required, string reason)
        {
            components.Add(component);
            foreach (var gate in required)
            {
                gates.Add(gate);
                if (reasons.TryGetValue(gate, out var existing))
                {
                    if (!existing.Contains(reason, StringComparison.Ordinal))
                    {
                        reasons[gate] = $"{existing}; {reason}";
                    }
                }
                else
                {
                    reasons.Add(gate, reason);
                }
            }
        }
    }

    public static string Name(IntegrationGate gate) => gate switch
    {
        IntegrationGate.AgentGitTests => "agent-git-tests",
        IntegrationGate.JobserverTests => "jobserver-tests",
        IntegrationGate.CSharpToolsTests => "csharp-tools-tests",
        IntegrationGate.ToolTests => "tool-tests",
        IntegrationGate.PowerShellChecks => "powershell-checks",
        IntegrationGate.PythonChecks => "python-checks",
        IntegrationGate.CMakeChecks => "cmake-checks",
        IntegrationGate.NativeTests => "native-tests",
        IntegrationGate.CodegenTests => "codegen-tests",
        IntegrationGate.UnrealTests => "unreal-tests",
        IntegrationGate.DevelopmentBuild => "development-build",
        IntegrationGate.BenchmarkBuild => "benchmark-build",
        _ => throw new ArgumentOutOfRangeException(nameof(gate), gate, "Unknown integration gate."),
    };

    private static IReadOnlyList<IntegrationGate> MinimumGates(string path)
    {
        if (path is ".integration-gates.json" or ".agent-git.json")
        {
            return global_gates;
        }
        if (path.EndsWith(".md", StringComparison.OrdinalIgnoreCase))
        {
            return [];
        }
        if (path.StartsWith("tools/AgentGit/", StringComparison.OrdinalIgnoreCase) ||
            path.StartsWith("tools/AgentGit.Tests/", StringComparison.OrdinalIgnoreCase))
        {
            return [IntegrationGate.AgentGitTests];
        }
        if (path.StartsWith("tools/GitSupport/", StringComparison.OrdinalIgnoreCase))
        {
            return [IntegrationGate.AgentGitTests, IntegrationGate.CSharpToolsTests];
        }
        if (path.StartsWith("tools/jobserver/", StringComparison.OrdinalIgnoreCase))
        {
            return [IntegrationGate.JobserverTests];
        }
        if (path.StartsWith("tools/", StringComparison.OrdinalIgnoreCase))
        {
            return [IntegrationGate.CSharpToolsTests];
        }
        if (path.Equals("dev.ps1", StringComparison.OrdinalIgnoreCase) ||
            path.StartsWith("PowerShell/", StringComparison.OrdinalIgnoreCase))
        {
            return [IntegrationGate.PowerShellChecks];
        }
        if (path.StartsWith("Scripts/", StringComparison.OrdinalIgnoreCase))
        {
            return [IntegrationGate.PythonChecks];
        }
        if (path.Equals("CMakeLists.txt", StringComparison.OrdinalIgnoreCase) ||
            path.Equals("CMakePresets.json", StringComparison.OrdinalIgnoreCase) ||
            path.StartsWith("cmake/", StringComparison.OrdinalIgnoreCase))
        {
            return path.Equals("cmake/presets/native.json", StringComparison.OrdinalIgnoreCase)
                ? [IntegrationGate.CMakeChecks, IntegrationGate.NativeTests]
                : path.Equals("cmake/presets/unreal.json", StringComparison.OrdinalIgnoreCase)
                    ? [IntegrationGate.CMakeChecks, IntegrationGate.UnrealTests, IntegrationGate.DevelopmentBuild]
                    : build_global_gates;
        }
        if (path.StartsWith("native/simulation_benchmark/", StringComparison.OrdinalIgnoreCase) ||
            path.StartsWith("tools/perf/", StringComparison.OrdinalIgnoreCase))
        {
            return [IntegrationGate.BenchmarkBuild];
        }
        if (path.StartsWith("native/", StringComparison.OrdinalIgnoreCase))
        {
            return [IntegrationGate.NativeTests];
        }
        if (path.StartsWith("Codegen/", StringComparison.OrdinalIgnoreCase) ||
            path.StartsWith("lispb/", StringComparison.OrdinalIgnoreCase))
        {
            return [IntegrationGate.CodegenTests, IntegrationGate.NativeTests];
        }
        if (path.StartsWith("Source/", StringComparison.OrdinalIgnoreCase) ||
            path.StartsWith("Plugins/", StringComparison.OrdinalIgnoreCase) ||
            path.StartsWith("unreal/", StringComparison.OrdinalIgnoreCase) ||
            path.StartsWith("Config/", StringComparison.OrdinalIgnoreCase) ||
            path.Equals("Sandbox.uproject", StringComparison.OrdinalIgnoreCase))
        {
            return [IntegrationGate.UnrealTests, IntegrationGate.DevelopmentBuild];
        }

        return [];
    }

    private static IReadOnlyList<ComponentRule> DefaultRules() =>
    [
        new("integration-policy", [".integration-gates.json", ".agent-git.json"], global_gates, []),
        new("docs", ["AGENTS.md", "README.md", "docs/", "PowerShell/README.md", "cmake/README.md"], [], []),
        new("agent-git", ["tools/AgentGit/", "tools/AgentGit.Tests/"], [IntegrationGate.AgentGitTests], []),
        new("git-support", ["tools/GitSupport/"], [], ["agent-git", "csharp-tools"]),
        new("jobserver", ["tools/jobserver/"], [IntegrationGate.JobserverTests], []),
        new("csharp-tools", ["tools/"], [IntegrationGate.CSharpToolsTests], []),
        new("powershell", ["dev.ps1", "PowerShell/"], [IntegrationGate.PowerShellChecks], []),
        new("python", ["Scripts/"], [IntegrationGate.PythonChecks], []),
        new("preset-generator", ["cmake/presets/"],
            [IntegrationGate.PythonChecks, IntegrationGate.CMakeChecks, IntegrationGate.NativeTests,
             IntegrationGate.UnrealTests, IntegrationGate.DevelopmentBuild], []),
        new("global-cmake", ["CMakeLists.txt", "CMakePresets.json", "cmake/compiler_", "cmake/jobserver_", "cmake/unreal"], build_global_gates, []),
        new("native", ["native/"], [IntegrationGate.NativeTests], []),
        new("codegen", ["Codegen/", "lispb/"], [IntegrationGate.CodegenTests, IntegrationGate.NativeTests], []),
        new("unreal", ["Source/", "Plugins/", "unreal/", "Config/", "Sandbox.uproject"],
            [IntegrationGate.UnrealTests, IntegrationGate.DevelopmentBuild], []),
        new("benchmark", ["tools/perf/", "native/simulation_benchmark/"], [IntegrationGate.BenchmarkBuild], []),
    ];

    private static IReadOnlyList<ComponentRule> ParseManifest(string json)
    {
        Manifest document;
        try
        {
            StrictJson.RejectDuplicateProperties(json);
            document = JsonSerializer.Deserialize<Manifest>(json, new JsonSerializerOptions
            {
                PropertyNamingPolicy = JsonNamingPolicy.CamelCase,
                PropertyNameCaseInsensitive = false,
                UnmappedMemberHandling = JsonUnmappedMemberHandling.Disallow,
            }) ?? throw new JsonException("Manifest contained null.");
        }
        catch (JsonException exception)
        {
            throw new PolicyConfigurationException(
                $"Integration gate manifest is malformed: {exception.Message}", exception);
        }
        if (document.Version != 1 || document.Components is null || document.Components.Count == 0)
        {
            throw new PolicyConfigurationException(
                "Integration gate manifest must use version 1 and declare components.");
        }

        var names = new HashSet<string>(StringComparer.Ordinal);
        var rules = new List<ComponentRule>();
        foreach (var component in document.Components)
        {
            if (string.IsNullOrWhiteSpace(component.Name) || !names.Add(component.Name) ||
                component.Paths is null || component.Paths.Count == 0 || component.Gates is null ||
                component.Affects is null)
            {
                throw new PolicyConfigurationException(
                    "Integration gate components require a unique name, paths, gates, and affects list.");
            }
            var gates = component.Gates.Select(ParseGate).ToArray();
            rules.Add(new ComponentRule(component.Name, component.Paths, gates, component.Affects));
        }
        foreach (var rule in rules)
        {
            if (rule.Affects.Any(name => !names.Contains(name)))
            {
                throw new PolicyConfigurationException(
                    $"Integration gate component '{rule.Name}' has an unknown affected component.");
            }
        }
        return rules;
    }

    private static IntegrationGate ParseGate(string value) => value switch
    {
        "agent-git-tests" => IntegrationGate.AgentGitTests,
        "jobserver-tests" => IntegrationGate.JobserverTests,
        "csharp-tools-tests" => IntegrationGate.CSharpToolsTests,
        "tool-tests" => IntegrationGate.ToolTests,
        "powershell-checks" => IntegrationGate.PowerShellChecks,
        "python-checks" => IntegrationGate.PythonChecks,
        "cmake-checks" => IntegrationGate.CMakeChecks,
        "native-tests" => IntegrationGate.NativeTests,
        "codegen-tests" => IntegrationGate.CodegenTests,
        "unreal-tests" => IntegrationGate.UnrealTests,
        "development-build" => IntegrationGate.DevelopmentBuild,
        "benchmark-build" => IntegrationGate.BenchmarkBuild,
        _ => throw new PolicyConfigurationException($"Unknown integration gate '{value}'."),
    };

    private sealed record Manifest(int Version, IReadOnlyList<ComponentDocument>? Components);

    private sealed record ComponentDocument(
        string Name,
        IReadOnlyList<string>? Paths,
        IReadOnlyList<string>? Gates,
        IReadOnlyList<string>? Affects);

    internal sealed record ComponentRule(
        string Name,
        IReadOnlyList<string> Prefixes,
        IReadOnlyList<IntegrationGate> Gates,
        IReadOnlyList<string> Affects)
    {
        public int MatchLength(string path) => Prefixes
            .Where(prefix =>
                path.Equals(prefix, StringComparison.OrdinalIgnoreCase) ||
                (prefix.EndsWith("/", StringComparison.Ordinal) &&
                 path.StartsWith(prefix, StringComparison.OrdinalIgnoreCase)) ||
                (!prefix.EndsWith("/", StringComparison.Ordinal) &&
                 prefix.Contains('_', StringComparison.Ordinal) &&
                 path.StartsWith(prefix, StringComparison.OrdinalIgnoreCase)))
            .Select(prefix => prefix.Length)
            .DefaultIfEmpty(-1)
            .Max();
    }
}
