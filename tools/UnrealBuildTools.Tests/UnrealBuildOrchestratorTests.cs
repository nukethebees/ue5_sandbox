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

        var exit_code = Program.Main(fixture.CreateArguments());

        Assert.AreEqual(7, exit_code);
    }

    [TestMethod]
    public void Build_passes_the_project_build_contract_to_ubt()
    {
        using var fixture = new UnrealBuildFixture();

        new UnrealBuildOrchestrator().Build(fixture.CreateRequest());

        var arguments = fixture.ReadBuildArguments();
        StringAssert.Contains(arguments, "SandboxEditor Win64 Development");
        StringAssert.Contains(arguments, $"-Project={fixture.ProjectPath}");
        StringAssert.Contains(arguments, "-WaitMutex");
        Assert.IsFalse(arguments.Contains("-Force", StringComparison.Ordinal));
        Assert.IsFalse(arguments.Contains("-NoEngineChanges", StringComparison.Ordinal));
    }

    [TestMethod]
    public void Build_preserves_spaced_paths_and_scopes_the_toolchain_to_the_child()
    {
        using var fixture = new UnrealBuildFixture();
        var original_toolchain = Environment.GetEnvironmentVariable("SANDBOX_NATIVE_TOOLCHAIN");
        Environment.SetEnvironmentVariable("SANDBOX_NATIVE_TOOLCHAIN", "parent-toolchain");
        try
        {
            new UnrealBuildOrchestrator().Build(fixture.CreateRequest());

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
    public void Build_reports_missing_build_scripts()
    {
        using var fixture = new UnrealBuildFixture();
        File.Delete(fixture.BuildScriptPath);

        var exception = Assert.ThrowsException<ToolInputException>(
            () => new UnrealBuildOrchestrator().Build(fixture.CreateRequest()));

        StringAssert.Contains(exception.Message, "build script file is missing");
    }

    [TestMethod]
    public void Build_reports_missing_projects()
    {
        using var fixture = new UnrealBuildFixture();
        File.Delete(fixture.ProjectPath);

        var exception = Assert.ThrowsException<ToolInputException>(
            () => new UnrealBuildOrchestrator().Build(fixture.CreateRequest()));

        StringAssert.Contains(exception.Message, "project file is missing");
    }

}
