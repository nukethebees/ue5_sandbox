namespace AgentGit;

// Semantic ownership is explicit. Physical C# registrations live in cmake/csharp_tests.cmake.
internal static class ToolComponents
{
    internal sealed record Component(string Name, string[] Paths, IntegrationGate Gate, string[] TestProjects);

    internal static readonly Component[] All =
    [
        new("agent-git", ["tools/AgentGit/", "tools/AgentGit.Tests/"], IntegrationGate.AgentGitTests,
            ["tools/AgentGit.Tests/AgentGit.Tests.csproj"]),
        new("agent-git-installer", ["tools/AgentGitInstaller/", "tools/AgentGitInstaller.Tests/"], IntegrationGate.CSharpToolsTests,
            ["tools/AgentGitInstaller.Tests/AgentGitInstaller.Tests.csproj"]),
        new("architecture-checks", ["tools/ArchitectureChecks/", "tools/ArchitectureChecks.Tests/"], IntegrationGate.CSharpToolsTests,
            ["tools/ArchitectureChecks.Tests/ArchitectureChecks.Tests.csproj"]),
        new("benchmark-tools", ["tools/BenchmarkTools/", "tools/BenchmarkTools.Tests/"], IntegrationGate.CSharpToolsTests,
            ["tools/BenchmarkTools.Tests/BenchmarkTools.Tests.csproj"]),
        new("code-format-tools", ["tools/CodeFormatTools/", "tools/CodeFormatTools.Tests/"], IntegrationGate.CSharpToolsTests,
            ["tools/CodeFormatTools.Tests/CodeFormatTools.Tests.csproj"]),
        new("game-package-tools", ["tools/GamePackageTools/", "tools/GamePackageTools.Tests/"], IntegrationGate.CSharpToolsTests,
            ["tools/GamePackageTools.Tests/GamePackageTools.Tests.csproj"]),
        new("git-tools", ["tools/GitTools/", "tools/GitTools.Tests/"], IntegrationGate.CSharpToolsTests,
            ["tools/GitTools.Tests/GitTools.Tests.csproj"]),
        new("native-binary-tools", ["tools/NativeBinaryTools/", "tools/NativeBinaryTools.Tests/"], IntegrationGate.CSharpToolsTests,
            ["tools/NativeBinaryTools.Tests/NativeBinaryTools.Tests.csproj"]),
        new("unreal-build-tools", ["tools/UnrealBuildTools/", "tools/UnrealBuildTools.Tests/"], IntegrationGate.CSharpToolsTests,
            ["tools/UnrealBuildTools.Tests/UnrealBuildTools.Tests.csproj"]),
        new("git-support", ["tools/GitSupport/"], IntegrationGate.CSharpToolsTests,
            ["tools/AgentGit.Tests/AgentGit.Tests.csproj", "tools/AgentGitInstaller.Tests/AgentGitInstaller.Tests.csproj",
             "tools/GitTools.Tests/GitTools.Tests.csproj"]),
        new("layout-planner", ["tools/layout_planner/"], IntegrationGate.LayoutPlannerTests, []),
        new("image-lab", ["tools/image_lab/"], IntegrationGate.ImageLabTests, []),
        new("jobserver", ["tools/jobserver/"], IntegrationGate.JobserverTests, []),
        new("rust-tools", ["tools/rust/"], IntegrationGate.RustToolsTests, []),
        new("perf-tools", ["tools/perf/"], IntegrationGate.BenchmarkBuild, []),
    ];

    internal static Component? Find(string path) => All.SingleOrDefault(component =>
        component.Paths.Any(prefix => path.StartsWith(prefix, StringComparison.OrdinalIgnoreCase)));

    internal static IReadOnlyList<string> SelectTestProjects(IReadOnlyList<string> changed_paths, bool agent_git_tests_selected)
    {
        var projects = new SortedSet<string>(StringComparer.Ordinal);
        var broad = false;
        foreach (var raw_path in changed_paths)
        {
            var path = raw_path.Replace('\\', '/');
            var component = Find(path);
            if (component is not null)
            {
                projects.UnionWith(component.TestProjects);
            }
            else if (new IntegrationGatePlanner().Plan([path]).Gates.Any(gate =>
                gate is IntegrationGate.CSharpToolsTests or IntegrationGate.ToolTests))
            {
                broad = true;
            }
        }

        // A broad gate can also be requested by policy for paths outside tools/.
        if (broad || projects.Count == 0)
        {
            projects.UnionWith(All.SelectMany(component => component.TestProjects));
        }
        if (agent_git_tests_selected)
        {
            projects.Remove("tools/AgentGit.Tests/AgentGit.Tests.csproj");
        }
        return projects.ToArray();
    }
}
