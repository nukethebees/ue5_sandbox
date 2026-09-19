using Microsoft.VisualStudio.TestTools.UnitTesting;
using UnrealBuildTools;

namespace UnrealBuildTools.Tests;

[TestClass]
public sealed class EditorModuleCompatibilityCheckerTests
{
    [TestMethod]
    public void Check_returns_no_problems_for_matching_build_ids()
    {
        using var fixture = new UnrealBuildFixture();
        fixture.WriteEditorVersion("editor-build");
        var manifest_path = fixture.ProjectManifestPath("Development");
        fixture.WriteManifest(manifest_path, "editor-build");

        var result = new EditorModuleCompatibilityChecker().Check(fixture.EngineRoot, [manifest_path]);

        Assert.AreEqual("editor-build", result.ExpectedBuildId);
        Assert.AreEqual(0, result.Problems.Count);
    }

    [TestMethod]
    public void Check_reports_mismatched_build_ids()
    {
        using var fixture = new UnrealBuildFixture();
        fixture.WriteEditorVersion("editor-build");
        var manifest_path = fixture.ProjectManifestPath("Development");
        fixture.WriteManifest(manifest_path, "module-build");

        var result = new EditorModuleCompatibilityChecker().Check(fixture.EngineRoot, [manifest_path]);

        Assert.AreEqual(1, result.Problems.Count);
        StringAssert.Contains(result.Problems[0], "module-build");
        StringAssert.Contains(result.Problems[0], "editor-build");
    }

    [TestMethod]
    public void Check_reports_missing_manifest_files()
    {
        using var fixture = new UnrealBuildFixture();
        fixture.WriteEditorVersion("editor-build");
        var manifest_path = fixture.ProjectManifestPath("Development");

        var result = new EditorModuleCompatibilityChecker().Check(fixture.EngineRoot, [manifest_path]);

        Assert.AreEqual(1, result.Problems.Count);
        StringAssert.Contains(result.Problems[0], "missing");
    }

    [DataTestMethod]
    [DataRow("{}")]
    [DataRow("{ \"BuildId\": \"  \" }")]
    public void Check_rejects_a_missing_or_blank_editor_build_id(string version_json)
    {
        using var fixture = new UnrealBuildFixture();
        fixture.WriteEditorVersionJson(version_json);

        var exception = Assert.ThrowsException<ToolInputException>(
            () => new EditorModuleCompatibilityChecker().Check(fixture.EngineRoot, []));

        StringAssert.Contains(exception.Message, "does not contain a BuildId");
    }
}
