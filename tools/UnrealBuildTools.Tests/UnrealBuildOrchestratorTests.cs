using Microsoft.VisualStudio.TestTools.UnitTesting;
using UnrealBuildTools;

namespace UnrealBuildTools.Tests;

[TestClass]
public sealed class UnrealBuildOrchestratorTests
{
    [TestMethod]
    public void Program_returns_the_usage_exit_code_for_invalid_arguments()
    {
        Assert.AreEqual(2, Program.Main([]));
    }

    [TestMethod]
    public void Program_returns_the_build_exit_code_for_a_failed_build_script()
    {
        using var fixture = new UnrealBuildFixture();
        fixture.WriteBuildScript("@echo off\r\nexit /b 7\r\n");

        var exit_code = Program.Main(
        [
            "--build-script", fixture.BuildScriptPath,
            "--target", "SandboxEditor",
            "--platform", "Win64",
            "--configuration", "Development",
            "--project", fixture.ProjectPath,
            "--native-toolchain", "fixture-toolchain",
        ]);

        Assert.AreEqual(5, exit_code);
    }

    [TestMethod]
    public void Build_adds_force_only_when_the_receipt_is_missing()
    {
        using var fixture = new UnrealBuildFixture();
        fixture.WriteEditorVersion("editor-build");

        var exception = Assert.ThrowsException<PostBuildCompatibilityException>(
            () => new UnrealBuildOrchestrator().Build(fixture.CreateRequest(verify_editor_modules: true)));

        StringAssert.Contains(exception.Message, "editor target receipt is missing");
        StringAssert.Contains(fixture.ReadBuildArguments(), "-Force");
    }

    [TestMethod]
    public void Build_omits_force_when_modules_are_compatible()
    {
        using var fixture = new UnrealBuildFixture();
        fixture.WriteEditorVersion("editor-build");
        fixture.WriteReceipt("SandboxEditor", "Development", "{ \"BuildProducts\": [] }");
        fixture.WriteManifest(fixture.ProjectManifestPath("Development"), "editor-build");

        var outcome = new UnrealBuildOrchestrator().Build(fixture.CreateRequest(verify_editor_modules: true));

        Assert.IsFalse(outcome.ForceRebuild);
        Assert.IsFalse(fixture.ReadBuildArguments().Contains("-Force", StringComparison.Ordinal));
    }

    [TestMethod]
    public void Build_treats_post_build_incompatibility_as_a_failure()
    {
        using var fixture = new UnrealBuildFixture();
        fixture.WriteEditorVersion("editor-build");
        fixture.WriteReceipt("SandboxEditor", "Development", "{ \"BuildProducts\": [] }");
        fixture.WriteManifest(fixture.ProjectManifestPath("Development"), "stale-build");

        var exception = Assert.ThrowsException<PostBuildCompatibilityException>(
            () => new UnrealBuildOrchestrator().Build(fixture.CreateRequest(verify_editor_modules: true)));

        StringAssert.Contains(exception.Message, "remain incompatible");
    }

    [TestMethod]
    public void Build_script_runner_preserves_spaced_paths_and_scopes_the_toolchain_to_the_child()
    {
        using var fixture = new UnrealBuildFixture();
        var original_toolchain = Environment.GetEnvironmentVariable("SANDBOX_NATIVE_TOOLCHAIN");
        Environment.SetEnvironmentVariable("SANDBOX_NATIVE_TOOLCHAIN", "parent-toolchain");
        try
        {
            new BuildScriptRunner().Run(
                BuildPaths.Resolve(fixture.CreateRequest(verify_editor_modules: false)),
                fixture.CreateRequest(verify_editor_modules: false),
                force_rebuild: false);

            Assert.AreEqual("parent-toolchain", Environment.GetEnvironmentVariable("SANDBOX_NATIVE_TOOLCHAIN"));
        }
        finally
        {
            Environment.SetEnvironmentVariable("SANDBOX_NATIVE_TOOLCHAIN", original_toolchain);
        }

        StringAssert.Contains(fixture.ReadBuildArguments(), fixture.ProjectPath);
        StringAssert.Contains(fixture.ReadBuildEnvironment(), "SANDBOX_NATIVE_TOOLCHAIN=fixture-toolchain");
        Assert.AreEqual(original_toolchain, Environment.GetEnvironmentVariable("SANDBOX_NATIVE_TOOLCHAIN"));
    }

    [TestMethod]
    public void Program_runs_the_cli_against_temporary_receipt_data()
    {
        using var fixture = new UnrealBuildFixture();
        fixture.WriteEditorVersion("editor-build");
        fixture.WriteReceipt("SandboxEditor", "Development", "{ \"BuildProducts\": [] }");
        fixture.WriteManifest(fixture.ProjectManifestPath("Development"), "editor-build");

        var exit_code = Program.Main(
        [
            "--build-script", fixture.BuildScriptPath,
            "--target", "SandboxEditor",
            "--platform", "Win64",
            "--configuration", "Development",
            "--project", fixture.ProjectPath,
            "--native-toolchain", "fixture-toolchain",
            "--verify-editor-modules",
        ]);

        Assert.AreEqual(0, exit_code);
    }
}
