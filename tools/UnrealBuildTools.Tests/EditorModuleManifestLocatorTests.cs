using Microsoft.VisualStudio.TestTools.UnitTesting;
using UnrealBuildTools;

namespace UnrealBuildTools.Tests;

[TestClass]
public sealed class EditorModuleManifestLocatorTests
{
    [DataTestMethod]
    [DataRow("Development")]
    [DataRow("development")]
    [DataRow("DEVELOPMENT")]
    public void GetManifestName_returns_the_development_name_regardless_of_case(string configuration)
    {
        Assert.AreEqual("UnrealEditor.modules", EditorModuleManifestLocator.GetManifestName(configuration));
    }

    [TestMethod]
    public void GetManifestName_returns_the_configuration_specific_name()
    {
        Assert.AreEqual("UnrealEditor-Win64-DebugGame.modules", EditorModuleManifestLocator.GetManifestName("DebugGame"));
    }

    [DataTestMethod]
    [DataRow("Development")]
    [DataRow("development")]
    [DataRow("DEVELOPMENT")]
    public void GetTargetReceiptName_returns_the_development_name_regardless_of_case(string configuration)
    {
        Assert.AreEqual("SandboxEditor.target", EditorModuleManifestLocator.GetTargetReceiptName("SandboxEditor", configuration));
    }

    [TestMethod]
    public void GetTargetReceiptName_returns_the_configuration_specific_name()
    {
        Assert.AreEqual("SandboxEditor-Win64-DebugGame.target", EditorModuleManifestLocator.GetTargetReceiptName("SandboxEditor", "DebugGame"));
    }

    [TestMethod]
    public void Discover_includes_the_project_manifest_and_plugin_manifests_only_once()
    {
        using var fixture = new UnrealBuildFixture();
        fixture.WriteReceipt(
            "SandboxEditor",
            "Development",
            """
            {
              "BuildProducts": [
                { "Path": "$(ProjectDir)/Plugins/Example/Binaries/Win64/Example.dll" },
                { "Path": "$(ProjectDir)/plugins/example/Binaries/Win64/Example.pdb" },
                { "Path": "$(ProjectDir)/PluginsElse/NotAPlugin.dll" },
                { "Path": "$(ProjectDir)/Source/NotAPlugin.dll" }
              ]
            }
            """);

        var result = new EditorModuleManifestLocator().Discover(
            fixture.ProjectRoot,
            "SandboxEditor",
            "Development");

        Assert.IsNull(result.Problem);
        Assert.AreEqual(2, result.Paths.Count);
        Assert.IsTrue(PathsEqual(result.Paths[0], fixture.ProjectManifestPath("Development")));
        Assert.IsTrue(PathsEqual(
            result.Paths[1],
            Path.Combine(fixture.ProjectRoot, "Plugins", "Example", "Binaries", "Win64", "UnrealEditor.modules")));
    }

    [TestMethod]
    public void Discover_expands_project_dir_in_plugin_build_products()
    {
        using var fixture = new UnrealBuildFixture();
        fixture.WriteReceipt(
            "SandboxEditor",
            "DebugGame",
            """
            { "BuildProducts": [
              { "Path": "$(ProjectDir)/Plugins/Example/Binaries/Win64/Example.dll" }
            ] }
            """);

        var result = new EditorModuleManifestLocator().Discover(fixture.ProjectRoot, "SandboxEditor", "DebugGame");

        Assert.IsTrue(result.Paths.Any(path => PathsEqual(
            path,
            Path.Combine(fixture.ProjectRoot, "Plugins", "Example", "Binaries", "Win64", "UnrealEditor-Win64-DebugGame.modules"))));
    }

    [TestMethod]
    public void Discover_returns_a_force_rebuild_problem_for_a_missing_receipt()
    {
        using var fixture = new UnrealBuildFixture();

        var result = new EditorModuleManifestLocator().Discover(fixture.ProjectRoot, "SandboxEditor", "Development");

        StringAssert.Contains(result.Problem, "editor target receipt is missing");
        Assert.AreEqual(0, result.Paths.Count);
    }

    [TestMethod]
    public void Discover_rejects_a_receipt_without_build_products()
    {
        using var fixture = new UnrealBuildFixture();
        fixture.WriteReceipt("SandboxEditor", "Development", "{}");

        var exception = Assert.ThrowsException<ToolInputException>(
            () => new EditorModuleManifestLocator().Discover(fixture.ProjectRoot, "SandboxEditor", "Development"));

        StringAssert.Contains(exception.Message, "BuildProducts");
    }

    private static bool PathsEqual(string left, string right)
    {
        return string.Equals(left, right, StringComparison.OrdinalIgnoreCase);
    }
}
