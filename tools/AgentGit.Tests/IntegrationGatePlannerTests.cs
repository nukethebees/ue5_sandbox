using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace AgentGit.Tests;

[TestClass]
public sealed class IntegrationGatePlannerTests
{
    [TestMethod]
    public void AgentGit_change_never_schedules_Unreal()
    {
        var plan = new IntegrationGatePlanner().Plan(["tools/AgentGit/IntegrationTransaction.cs"]);

        CollectionAssert.AreEqual(new[] { IntegrationGate.AgentGitTests, IntegrationGate.CSharpToolsTests }, plan.Gates.ToArray());
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

        CollectionAssert.AreEqual(new[] { IntegrationGate.AgentGitTests, IntegrationGate.CSharpToolsTests }, plan.Gates.ToArray());
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
            var commands = IntegrationGateValidator.Commands(gate, plan);
            CollectionAssert.AreEqual(new[] { "--workflow", "--preset", preset }, commands.Single().Arguments.ToArray());
        }
    }

    [TestMethod]
    public void Every_CSharp_tool_has_explicit_project_selection()
    {
        foreach (var name in new[] { "AgentGitInstaller", "ArchitectureChecks", "BenchmarkTools", "CodeFormatTools",
            "GamePackageTools", "GitTools", "NativeBinaryTools", "UnrealBuildTools" })
        {
            var project = $"tools/{name}.Tests/{name}.Tests.csproj";
            var plan = new IntegrationGatePlanner().Plan([$"tools/{name}/changed.cs"]);
            CollectionAssert.AreEqual(new[] { project }, plan.TestProjects[IntegrationGate.CSharpToolsTests].ToArray());
            var commands = IntegrationGateValidator.Commands(IntegrationGate.CSharpToolsTests, plan);
            Assert.AreEqual("build", commands[0].Arguments[0]);
            Assert.AreEqual(project, commands[0].Arguments[1]);
            Assert.AreEqual("test", commands[1].Arguments[0]);
            Assert.AreEqual(project, commands[1].Arguments[1]);
            CollectionAssert.Contains(commands[1].Arguments.ToArray(), "--no-build");
            CollectionAssert.Contains(commands[1].Arguments.ToArray(), "--no-restore");
            Assert.IsFalse(commands.Any(command => command.Arguments.Contains("tools/Tools.slnx")));
        }
    }

    [TestMethod]
    public void GitSupport_selects_all_transitive_consumers_without_repeating_AgentGit()
    {
        var plan = new IntegrationGatePlanner().Plan(["tools/GitSupport/Worktree.cs"]);
        CollectionAssert.AreEqual(new[]
        {
            "tools/AgentGitInstaller.Tests/AgentGitInstaller.Tests.csproj",
            "tools/GitTools.Tests/GitTools.Tests.csproj",
        }, plan.TestProjects[IntegrationGate.CSharpToolsTests].ToArray());
        CollectionAssert.AreEqual(new[] { "tools/AgentGit.Tests/AgentGit.Tests.csproj" },
            plan.TestProjects[IntegrationGate.AgentGitTests].ToArray());
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
                Assert.AreEqual(8, plan.TestProjects[IntegrationGate.CSharpToolsTests].Count, shared);
                Assert.AreEqual(1, plan.TestProjects[IntegrationGate.AgentGitTests].Count, shared);
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
    }

    [TestMethod]
    public void Manifest_cannot_remove_native_tool_safety_gate()
    {
        var planner = new IntegrationGatePlanner(
            [new IntegrationGatePlanner.ComponentRule("layout-planner", ["tools/layout_planner/"], [], [])]);
        CollectionAssert.AreEqual(new[] { IntegrationGate.LayoutPlannerTests },
            planner.Plan(["tools/layout_planner/app/editor.cpp"]).Gates.ToArray());
    }

    [TestMethod]
    public void Simulation_does_not_select_standalone_native_tools_but_shared_consumers_do()
    {
        var simulation = new IntegrationGatePlanner().Plan(["native/simulation/src/world.cpp"]);
        CollectionAssert.AreEqual(new[] { IntegrationGate.NativeTests }, simulation.Gates.ToArray());
        foreach (var (path, consumer) in new[]
        {
            ("native/layout/lib/include/layout.h", IntegrationGate.LayoutPlannerTests),
            ("native/image/include/image.h", IntegrationGate.ImageLabTests),
            ("native/lispb/src/project.cpp", IntegrationGate.LayoutPlannerTests),
        })
        {
            var plan = new IntegrationGatePlanner().Plan([path]);
            CollectionAssert.Contains(plan.Gates.ToArray(), consumer);
            CollectionAssert.DoesNotContain(plan.Gates.ToArray(), IntegrationGate.CSharpToolsTests);
        }
        var broad = new IntegrationGatePlanner().Plan(
            ["tools/layout_planner/app/editor.cpp", "tools/image_lab/lib/image.cpp", "tools/perf/tests/test.cpp"], true);
        CollectionAssert.AreEqual(new[] { IntegrationGate.ToolTests, IntegrationGate.LayoutPlannerTests,
            IntegrationGate.ImageLabTests, IntegrationGate.BenchmarkBuild }, broad.Gates.ToArray());
        var broad_commands = IntegrationGateValidator.Commands(IntegrationGate.ToolTests, broad);
        CollectionAssert.AreEqual(new[] { "--preset", "tool-tests", "-LE", "^(layout|image-lab|perf)$" },
            broad_commands.Last().Arguments.ToArray());
        CollectionAssert.AreEqual(new[] { "--workflow", "--preset", "layout-planner" },
            IntegrationGateValidator.Commands(IntegrationGate.LayoutPlannerTests, broad).Single().Arguments.ToArray());
    }

    [TestMethod]
    public void Infrastructure_changes_schedule_and_execute_CMake_regressions()
    {
        foreach (var path in new[] { "cmake/csharp_tests.cmake", "cmake/csharp_tests.py", "cmake/test_csharp_tests.py",
            "tools/AgentGit/IntegrationServices.cs", "tools/AgentGit/IntegrationGates.cs", ".integration-gates.json",
            "tools/Tools.slnx", "tools/Directory.Build.props", "tools/Directory.Build.targets",
            "tools/CodeFormatTools/CodeFormatTools.csproj", "Scripts/CMakeLists.txt", "tools/CMakeLists.txt",
            "native/simulation/target_policy.cmake" })
        {
            var plan = new IntegrationGatePlanner().Plan([path]);
            CollectionAssert.Contains(plan.Gates.ToArray(), IntegrationGate.CMakeChecks, path);
            var commands = IntegrationGateValidator.Commands(IntegrationGate.CMakeChecks, plan);
            CollectionAssert.AreEqual(new[] { "cmake/presets/generate.py", "--check" }, commands[0].Arguments.ToArray());
            CollectionAssert.AreEqual(new[] { "--preset", "native" }, commands[1].Arguments.ToArray());
            Assert.AreEqual("ctest", commands[2].Executable);
            CollectionAssert.AreEqual(new[] { "--test-dir", "out/build/native", "-L", "^cmake$", "-E", "^CMake[.]Presets$",
                "--output-on-failure", "--no-tests=error" }, commands[2].Arguments.ToArray());
        }
    }

    [TestMethod]
    public void All_owned_Python_locations_use_static_analysis_of_the_complete_CMake_tree()
    {
        foreach (var path in new[] { "Scripts/new.py", "cmake/csharp_tests.py", "cmake/test_csharp_tests.py",
            "cmake/presets/new.py", "cmake/future/new.py" })
        {
            var plan = new IntegrationGatePlanner().Plan([path]);
            CollectionAssert.Contains(plan.Gates.ToArray(), IntegrationGate.PythonChecks, path);
            var commands = IntegrationGateValidator.Commands(IntegrationGate.PythonChecks, plan);
            CollectionAssert.AreEqual(new[] { "Scripts", "cmake" }, commands[0].Arguments.ToArray());
            CollectionAssert.AreEqual(new[] { "check", "Scripts", "cmake" }, commands[1].Arguments.ToArray());
        }
    }

    [TestMethod]
    public void Every_raw_CTest_gate_rejects_zero_selected_tests()
    {
        var plan = new IntegrationGatePlanner().Plan(["CMakeLists.txt"]);
        var commands = Enum.GetValues<IntegrationGate>().SelectMany(gate => IntegrationGateValidator.Commands(gate, plan));
        var ctest = commands.Where(command => command.Executable == "ctest" && command.Arguments.Contains("-L")).ToArray();
        Assert.AreEqual(4, ctest.Length);
        foreach (var command in ctest)
        {
            CollectionAssert.Contains(command.Arguments.ToArray(), "--no-tests=error");
        }
    }

    [TestMethod]
    public void Installed_policy_preserves_projects_and_consumers_even_if_pinned_policy_omits_them()
    {
        var planner = new IntegrationGatePlanner(
            [new IntegrationGatePlanner.ComponentRule("git-support", ["tools/GitSupport/"], [], [])]);
        var plan = planner.Plan(["tools/GitSupport/Worktree.cs"]);
        CollectionAssert.AreEqual(new[] { "tools/AgentGitInstaller.Tests/AgentGitInstaller.Tests.csproj",
            "tools/GitTools.Tests/GitTools.Tests.csproj" }, plan.TestProjects[IntegrationGate.CSharpToolsTests].ToArray());
        CollectionAssert.Contains(plan.Gates.ToArray(), IntegrationGate.AgentGitTests);
    }

    [TestMethod]
    public void Legacy_manifest_without_project_metadata_cannot_narrow_shared_validation()
    {
        var planner = new IntegrationGatePlanner(
            [new IntegrationGatePlanner.ComponentRule("legacy", ["tools/"], [IntegrationGate.CSharpToolsTests], [])]);
        var plan = planner.Plan(["tools/CodeFormatTools/Source.cs", "tools/Directory.Build.props"]);
        Assert.AreEqual(8, plan.TestProjects[IntegrationGate.CSharpToolsTests].Count);
    }

    [TestMethod]
    public async Task Project_selection_and_consumers_come_from_pinned_policy_not_worktree()
    {
        using var fixture = new TemporaryAgentGitRepository();
        fixture.WriteFile(".integration-gates.json", """
            { "version": 1, "components": [
              { "name": "formatter", "paths": ["tools/CodeFormatTools/"], "gates": ["csharp-tools-tests"],
                "testProjects": ["tools/BenchmarkTools.Tests/BenchmarkTools.Tests.csproj"], "affects": ["git"] },
              { "name": "git", "paths": ["tools/GitTools/"], "gates": ["csharp-tools-tests"],
                "testProjects": ["tools/GitTools.Tests/GitTools.Tests.csproj"], "affects": [] }
            ] }
            """);
        fixture.RunGit("add", ".integration-gates.json");
        fixture.RunGit("commit", "-qm", "pin project ownership");
        var commit = fixture.RunGit("rev-parse", "HEAD").Trim();
        fixture.WriteFile(".integration-gates.json", "{ untrusted broken policy }");
        var planner = await IntegrationGatePlanner.LoadAsync(new GitClient(fixture.Trust, new ProcessRunner()),
            fixture.RepositoryRoot, commit, default);
        var plan = planner.Plan(["tools/CodeFormatTools/Source.cs"]);
        CollectionAssert.AreEqual(new[] { "tools/BenchmarkTools.Tests/BenchmarkTools.Tests.csproj",
            "tools/CodeFormatTools.Tests/CodeFormatTools.Tests.csproj", "tools/GitTools.Tests/GitTools.Tests.csproj" },
            plan.TestProjects[IntegrationGate.CSharpToolsTests].ToArray());
    }

}
