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
    LayoutPlannerTests,
    ImageLabTests,
    RustToolsTests,
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
    IReadOnlyDictionary<IntegrationGate, string> Reasons,
    IReadOnlyDictionary<IntegrationGate, IReadOnlyList<string>> TestProjects)
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
        IntegrationGate.ToolTests,
        IntegrationGate.NativeTests,
        IntegrationGate.CodegenTests,
        IntegrationGate.UnrealTests,
        IntegrationGate.DevelopmentBuild,
    ];

    // This immutable installed snapshot is the minimum policy, never the feature worktree.
    private static readonly IReadOnlyList<ComponentRule> minimum_rules = LoadMinimumRules();

    private readonly IReadOnlyList<ComponentRule> rules;

    public IntegrationGatePlanner() : this(minimum_rules)
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
        var csharp_projects = new SortedSet<string>(StringComparer.Ordinal);
        var agent_git_projects = new SortedSet<string>(StringComparer.Ordinal);
        foreach (var raw_path in changed_paths)
        {
            var path = raw_path.Replace('\\', '/');
            if (path.StartsWith("tools/", StringComparison.OrdinalIgnoreCase) &&
                path.EndsWith(".csproj", StringComparison.OrdinalIgnoreCase))
            {
                Add("csharp-project-graph", [IntegrationGate.CMakeChecks], "validate changed MSBuild project ownership");
            }
            Expand(path, minimum_rules, "built-in safety policy");
            if (!ReferenceEquals(rules, minimum_rules))
            {
                Expand(path, rules, "pinned base policy");
            }
        }

        if (include_tool_tests)
        {
            Add("explicit-tool-tests", [IntegrationGate.ToolTests], "requested with --tool-tests");
        }
        if (gates.Contains(IntegrationGate.ToolTests))
        {
            foreach (var covered in new[] { IntegrationGate.AgentGitTests, IntegrationGate.CSharpToolsTests,
                IntegrationGate.RustToolsTests })
            {
                gates.Remove(covered);
                reasons.Remove(covered);
            }
        }
        if (gates.Contains(IntegrationGate.AgentGitTests))
        {
            csharp_projects.ExceptWith(agent_git_projects);
        }

        return new IntegrationGatePlan(
            components.ToArray(),
            gates.OrderBy(value => value).ToArray(),
            reasons,
            new Dictionary<IntegrationGate, IReadOnlyList<string>>
            {
                [IntegrationGate.AgentGitTests] = agent_git_projects.ToArray(),
                [IntegrationGate.CSharpToolsTests] = csharp_projects.ToArray(),
            });

        void Expand(string path, IReadOnlyList<ComponentRule> policy, string source)
        {
            var matched = policy
                .Select(rule => (Rule: rule, Length: rule.MatchLength(path)))
                .Where(value => value.Length >= 0)
                .ToArray();
            var longest = matched.Length == 0 ? -1 : matched.Max(value => value.Length);
            var matches = matched.Where(value => value.Length == longest).Select(value => value.Rule).ToArray();
            if (matches.Length == 0)
            {
                Add("unclassified", global_gates, $"'{path}' is not owned by a known component");
                AddAllProjects();
                return;
            }

            var pending = new Queue<(ComponentRule Rule, string Reason)>(matches.Select(match =>
                (match, $"'{path}' belongs to {match.Name}")));
            var expanded = new HashSet<string>(StringComparer.Ordinal);
            var projects = new HashSet<string>(StringComparer.Ordinal);
            var needs_csharp = false;
            while (pending.TryDequeue(out var current))
            {
                if (!expanded.Add(current.Rule.Name))
                {
                    continue;
                }

                Add(current.Rule.Name, current.Rule.Gates, $"{source}: {current.Reason}");
                projects.UnionWith(current.Rule.TestProjects ?? []);
                needs_csharp |= current.Rule.Gates.Contains(IntegrationGate.CSharpToolsTests);
                if (current.Rule.Gates.Contains(IntegrationGate.AgentGitTests))
                {
                    agent_git_projects.UnionWith(current.Rule.TestProjects ?? []);
                }
                foreach (var affected_name in current.Rule.Affects)
                {
                    var affected = policy.SingleOrDefault(rule => rule.Name == affected_name)
                        ?? throw new PolicyConfigurationException(
                            $"Integration gate component '{current.Rule.Name}' affects unknown component '{affected_name}'.");
                    pending.Enqueue((affected, $"{affected.Name} depends on {current.Rule.Name}"));
                }
            }
            csharp_projects.UnionWith(projects);
            // Older trusted manifests can require C# coverage without declaring projects.
            // Widen each such path independently so another narrow path cannot hide it.
            if (needs_csharp && projects.Count == 0)
            {
                AddAllProjects();
            }
        }

        void AddAllProjects()
        {
            csharp_projects.UnionWith(minimum_rules.Concat(rules).SelectMany(rule => rule.TestProjects ?? []));
            agent_git_projects.UnionWith(minimum_rules.Where(rule => rule.Gates.Contains(IntegrationGate.AgentGitTests))
                .SelectMany(rule => rule.TestProjects ?? []));
        }

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
        IntegrationGate.LayoutPlannerTests => "layout-planner-tests",
        IntegrationGate.ImageLabTests => "image-lab-tests",
        IntegrationGate.RustToolsTests => "rust-tools-tests",
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

    private static IReadOnlyList<ComponentRule> LoadMinimumRules()
    {
        using var stream = typeof(IntegrationGatePlanner).Assembly.GetManifestResourceStream(
            "AgentGit.integration-gates.json")
            ?? throw new PolicyConfigurationException("Installed integration safety policy is missing.");
        using var reader = new StreamReader(stream);
        return ParseManifest(reader.ReadToEnd());
    }

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
            if (component.TestProjects?.Any(project => !project.StartsWith("tools/", StringComparison.Ordinal) ||
                !project.EndsWith(".Tests.csproj", StringComparison.Ordinal) || project.Contains("..", StringComparison.Ordinal) ||
                project.Contains('\\', StringComparison.Ordinal)) == true)
            {
                throw new PolicyConfigurationException($"Invalid test project in component '{component.Name}'.");
            }
            rules.Add(new ComponentRule(component.Name, component.Paths, gates, component.Affects, component.TestProjects));
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
        "layout-planner-tests" => IntegrationGate.LayoutPlannerTests,
        "image-lab-tests" => IntegrationGate.ImageLabTests,
        "rust-tools-tests" => IntegrationGate.RustToolsTests,
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
        IReadOnlyList<string>? Affects,
        IReadOnlyList<string>? TestProjects);

    internal sealed record ComponentRule(
        string Name,
        IReadOnlyList<string> Prefixes,
        IReadOnlyList<IntegrationGate> Gates,
        IReadOnlyList<string> Affects,
        IReadOnlyList<string>? TestProjects = null)
    {
        public int MatchLength(string path) => Prefixes
            .Where(prefix =>
                path.Equals(prefix, StringComparison.OrdinalIgnoreCase) ||
                (prefix.EndsWith("/", StringComparison.Ordinal) &&
                 path.StartsWith(prefix, StringComparison.OrdinalIgnoreCase)))
            .Select(prefix => prefix.Length)
            .DefaultIfEmpty(-1)
            .Max();
    }
}
