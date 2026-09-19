using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace ArchitectureChecks.Tests;

[TestClass]
public sealed class BuildCsDependencyParserTests
{
    [TestMethod]
    public void Parse_reads_public_and_private_lists_and_single_additions()
    {
        var dependencies = new BuildCsDependencyParser().Parse(
            """
            PublicDependencyModuleNames.AddRange(new string[] { "Core", "NativeSimulation" });
            PublicDependencyModuleNames.Add("Engine");
            PrivateDependencyModuleNames.Add("SpaceGamePresentation");
            PrivateDependencyModuleNames.AddRange(new string[] { "SandboxCore" });
            """);

        CollectionAssert.AreEquivalent(
            new[] { "Core", "NativeSimulation", "Engine", "SpaceGamePresentation", "SandboxCore" },
            dependencies.ToArray());
    }

    [TestMethod]
    public void Parse_ignores_line_and_block_comments()
    {
        var dependencies = new BuildCsDependencyParser().Parse(
            """
            // PublicDependencyModuleNames.Add("IgnoredLine");
            /* PrivateDependencyModuleNames.AddRange(new string[] { "IgnoredBlock" }); */
            PublicDependencyModuleNames.Add("Core");
            """);

        CollectionAssert.AreEquivalent(new[] { "Core" }, dependencies.ToArray());
    }
}
