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
        CollectionAssert.Contains(plan.Components.ToArray(), "git-tools");
        CollectionAssert.Contains(plan.Components.ToArray(), "agent-git-installer");
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
    public void Rust_tool_uses_tool_tests_without_CSharp_or_Unreal()
    {
        var plan = new IntegrationGatePlanner().Plan(
            ["tools/rust/crates/set-live-coding-disabled/src/lib.rs"]);

        CollectionAssert.AreEqual(new[] { IntegrationGate.RustToolsTests }, plan.Gates.ToArray());
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

    [TestMethod]
    public void Native_tools_select_their_native_build_and_test_workflows()
    {
        foreach (var (path, gate, preset) in new[]
        {
            ("tools/layout_planner/app/editor.cpp", IntegrationGate.LayoutPlannerTests, "layout-planner"),
            ("tools/image_lab/lib/image.cpp", IntegrationGate.ImageLabTests, "image-lab"),
        })
        {
            var plan = new IntegrationGatePlanner().Plan([path]);
            CollectionAssert.AreEqual(new[] { gate }, plan.Gates.ToArray());
            var commands = IntegrationGateValidator.Commands(gate, [path], false);
            CollectionAssert.AreEqual(new[] { "--workflow", "--preset", preset }, commands.Single().Arguments.ToArray());
        }
    }

    [TestMethod]
    public void Every_CSharp_tool_has_explicit_project_selection()
    {
        foreach (var tool in ToolComponents.All.Where(tool => tool.TestProjects.Length == 1))
        {
            var paths = new[] { tool.Paths[0] + "changed.cs" };
            CollectionAssert.AreEqual(tool.TestProjects, ToolComponents.SelectTestProjects(paths, false).ToArray());
            var commands = IntegrationGateValidator.Commands(tool.Gate, paths, false);
            Assert.AreEqual("build", commands[0].Arguments[0]);
            Assert.AreEqual("test", commands[1].Arguments[0]);
            CollectionAssert.Contains(commands[1].Arguments.ToArray(), "--no-build");
            CollectionAssert.Contains(commands[1].Arguments.ToArray(), "--no-restore");
            Assert.IsFalse(commands.Any(command => command.Arguments.Contains("tools/Tools.slnx")));
        }
    }

    [TestMethod]
    public void GitSupport_selects_all_transitive_consumers_without_repeating_AgentGit()
    {
        CollectionAssert.AreEqual(new[]
        {
            "tools/AgentGitInstaller.Tests/AgentGitInstaller.Tests.csproj",
            "tools/GitTools.Tests/GitTools.Tests.csproj",
        }, ToolComponents.SelectTestProjects(["tools/GitSupport/Worktree.cs"], true).ToArray());
    }

    [TestMethod]
    public void Shared_infrastructure_widens_even_when_a_specific_project_is_also_changed()
    {
        foreach (var shared in new[] { "tools/Directory.Build.props", "tools/Directory.Build.targets", "tools/Tools.slnx", "CMakeLists.txt" })
        {
            foreach (var paths in new[]
            {
                new[] { shared, "tools/BenchmarkTools/Program.cs" },
                new[] { "tools/BenchmarkTools/Program.cs", shared },
            })
            {
                var plan = new IntegrationGatePlanner().Plan(paths);
                CollectionAssert.Contains(plan.Gates.ToArray(), IntegrationGate.CSharpToolsTests);
                Assert.AreEqual(9, ToolComponents.SelectTestProjects(paths, false).Count, shared);
                Assert.AreEqual(8, ToolComponents.SelectTestProjects(paths, true).Count, shared);
            }
        }
    }

    [TestMethod]
    public void Unknown_tools_keep_broad_tool_and_native_coverage()
    {
        var plan = new IntegrationGatePlanner().Plan(
            ["tools/new-tool/new.file", "tools/AgentGit/Program.cs", "tools/rust/Cargo.toml"]);
        CollectionAssert.Contains(plan.Gates.ToArray(), IntegrationGate.ToolTests);
        CollectionAssert.Contains(plan.Gates.ToArray(), IntegrationGate.NativeTests);
        CollectionAssert.DoesNotContain(plan.Gates.ToArray(), IntegrationGate.AgentGitTests);
        CollectionAssert.DoesNotContain(plan.Gates.ToArray(), IntegrationGate.RustToolsTests);
        CollectionAssert.DoesNotContain(plan.Gates.ToArray(), IntegrationGate.CSharpToolsTests);
        Assert.AreEqual(9, ToolComponents.SelectTestProjects(
            ["tools/new-tool/new.file", "tools/BenchmarkTools/Program.cs"], false).Count);
    }

    [TestMethod]
    public void Manifest_cannot_remove_native_tool_safety_gate()
    {
        var planner = new IntegrationGatePlanner(
            [new IntegrationGatePlanner.ComponentRule("layout-planner", ["tools/layout_planner/"], [], [])]);
        CollectionAssert.AreEqual(new[] { IntegrationGate.LayoutPlannerTests },
            planner.Plan(["tools/layout_planner/app/editor.cpp"]).Gates.ToArray());
    }

}
