using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace AgentGit.Tests;

[TestClass]
public sealed class IntegrationGatePlannerTests
{
    [TestMethod]
    public void AgentGit_change_never_schedules_Unreal()
    {
        var plan = new IntegrationGatePlanner().Plan(["tools/AgentGit/IntegrationTransaction.cs"]);

        CollectionAssert.AreEqual(new[] { IntegrationGate.AgentGitTests }, plan.Gates.ToArray());
        Assert.IsFalse(plan.RequiresUnreal);
    }

    [TestMethod]
    public void Unreal_change_schedules_Unreal_and_Development()
    {
        var plan = new IntegrationGatePlanner().Plan(["Source/Sandbox/SpaceGame.cpp"]);

        CollectionAssert.Contains(plan.Gates.ToArray(), IntegrationGate.UnrealTests);
        CollectionAssert.Contains(plan.Gates.ToArray(), IntegrationGate.DevelopmentBuild);
        Assert.IsTrue(plan.RequiresUnreal);
    }

    [TestMethod]
    public void Global_CMake_and_unknown_changes_widen_conservatively()
    {
        foreach (var path in new[] { "CMakeLists.txt", "mystery/new.file" })
        {
            var plan = new IntegrationGatePlanner().Plan([path]);
            CollectionAssert.Contains(plan.Gates.ToArray(), IntegrationGate.NativeTests, path);
            CollectionAssert.Contains(plan.Gates.ToArray(), IntegrationGate.UnrealTests, path);
            CollectionAssert.Contains(plan.Gates.ToArray(), IntegrationGate.DevelopmentBuild, path);
        }
    }

    [TestMethod]
    public void Documentation_only_change_uses_sanity_checks_only()
    {
        var plan = new IntegrationGatePlanner().Plan(["docs/agent-git.md"]);

        Assert.AreEqual(0, plan.Gates.Count);
        Assert.IsFalse(plan.RequiresUnreal);
    }

    [TestMethod]
    public void Shared_component_expands_to_declared_dependants()
    {
        var plan = new IntegrationGatePlanner().Plan(["tools/GitSupport/Worktree.cs"]);

        CollectionAssert.Contains(plan.Components.ToArray(), "git-support");
        CollectionAssert.Contains(plan.Components.ToArray(), "agent-git");
        CollectionAssert.Contains(plan.Components.ToArray(), "csharp-tools");
        CollectionAssert.Contains(plan.Gates.ToArray(), IntegrationGate.AgentGitTests);
        CollectionAssert.Contains(plan.Gates.ToArray(), IntegrationGate.CSharpToolsTests);
        Assert.IsFalse(plan.RequiresUnreal);
    }

    [TestMethod]
    public void Standalone_CSharp_tool_uses_tooling_gate_without_Unreal()
    {
        var plan = new IntegrationGatePlanner().Plan(["tools/CodeFormatTools/FormatApplication.cs"]);

        CollectionAssert.AreEqual(new[] { IntegrationGate.CSharpToolsTests }, plan.Gates.ToArray());
        Assert.IsFalse(plan.RequiresUnreal);
    }

    [TestMethod]
    public async Task Planner_uses_manifest_from_pinned_base_not_feature_worktree()
    {
        using var fixture = new TemporaryAgentGitRepository();
        const string trusted_manifest = """
            {
              "version": 1,
              "components": [
                { "name": "agent-git", "paths": ["tools/AgentGit/"], "gates": ["agent-git-tests"], "affects": [] }
              ]
            }
            """;
        fixture.WriteFile(".integration-gates.json", trusted_manifest);
        fixture.RunGit("add", ".integration-gates.json");
        fixture.RunGit("commit", "-qm", "add trusted gates");
        var base_commit = fixture.RunGit("rev-parse", "HEAD").Trim();
        fixture.WriteFile(".integration-gates.json", """
            {
              "version": 1,
              "components": [
                { "name": "agent-git", "paths": ["tools/AgentGit/"], "gates": [], "affects": [] }
              ]
            }
            """);

        var git = new GitClient(fixture.Trust, new ProcessRunner());
        var planner = await IntegrationGatePlanner.LoadAsync(
            git, fixture.RepositoryRoot, base_commit, default);
        var plan = planner.Plan(["tools/AgentGit/Program.cs"]);

        CollectionAssert.AreEqual(new[] { IntegrationGate.AgentGitTests }, plan.Gates.ToArray());
    }

    [TestMethod]
    public void Manifest_cannot_remove_built_in_minimum_gate()
    {
        var planner = new IntegrationGatePlanner(
        [
            new IntegrationGatePlanner.ComponentRule("agent-git", ["tools/AgentGit/"], [], []),
        ]);

        var plan = planner.Plan(["tools/AgentGit/Program.cs"]);

        CollectionAssert.Contains(plan.Gates.ToArray(), IntegrationGate.AgentGitTests);
    }
}
