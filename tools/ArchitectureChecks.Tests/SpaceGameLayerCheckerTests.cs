using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace ArchitectureChecks.Tests;

[TestClass]
public sealed class SpaceGameLayerCheckerTests
{
    [TestMethod]
    public void Check_accepts_allowed_dependencies_and_neutral_simulation_output()
    {
        using var fixture = new ArchitectureCheckFixture();
        fixture.WriteBuildCs(
            "SpaceGameSimulation",
            "PublicDependencyModuleNames.AddRange(new string[] { \"Core\", \"CoreUObject\", \"Engine\", \"TraceLog\", \"NativeMemory\", \"NativeSimulation\", \"SandboxCore\", \"SandboxCoreEngine\" });");
        fixture.WriteBuildCs("SpaceGamePresentation", "PrivateDependencyModuleNames.Add(\"SpaceGameSimulation\");");
        fixture.WriteBuildCs("SpaceGame", "PublicDependencyModuleNames.AddRange(new string[] { \"SpaceGameSimulation\", \"SpaceGamePresentation\" });");
        fixture.WriteSimulationSource("Public/Output.h", "TConstArrayView<FEntityFrameChange> changes;");

        var result = Check(fixture);

        Assert.IsTrue(result.IsValid, string.Join(Environment.NewLine, result.Diagnostics));
    }

    [DataTestMethod]
    [DataRow("#include <SpaceGamePresentation/presentation/LevelPresentation.h>")]
    [DataRow("#include \"SpaceGame/simulation/TestBatchOrchestrator.h\"")]
    [DataRow("friend struct FLevelPresentation;")]
    [DataRow("TOptional<FLevelPresentation> output;")]
    [DataRow("class UNiagaraComponent;")]
    [DataRow("#include <SandboxGameShared/utilities/actor_utils.h>")]
    public void Check_rejects_existing_forbidden_reference_patterns(string contents)
    {
        using var fixture = new ArchitectureCheckFixture();
        fixture.WriteValidComposition();
        fixture.WriteSimulationSource("Private/Forbidden.cpp", contents);

        var result = Check(fixture);

        CollectionAssert.AreEqual(
            new[] { Path.Combine("Plugins", "SpaceGame", "Source", "SpaceGameSimulation", "Private", "Forbidden.cpp") + ":1: forbidden layer reference" },
            result.Diagnostics.ToArray());
    }

    [TestMethod]
    public void Check_rejects_forbidden_simulation_references_with_line_numbers()
    {
        using var fixture = new ArchitectureCheckFixture();
        fixture.WriteValidComposition();
        fixture.WriteSimulationSource(
            "Private/Boundary.cpp",
            """
            #include <Core.h>
            #include <SpaceGamePresentation/presentation/LevelPresentation.h>
            class UNiagaraComponent;
            """);

        var result = Check(fixture);

        CollectionAssert.AreEqual(
            new[]
            {
                Path.Combine("Plugins", "SpaceGame", "Source", "SpaceGameSimulation", "Private", "Boundary.cpp") + ":2: forbidden layer reference",
                Path.Combine("Plugins", "SpaceGame", "Source", "SpaceGameSimulation", "Private", "Boundary.cpp") + ":3: forbidden layer reference",
            },
            result.Diagnostics.ToArray());
    }

    [TestMethod]
    public void Check_traverses_dependencies_and_reports_forbidden_transitive_dependency()
    {
        using var fixture = new ArchitectureCheckFixture();
        fixture.WriteValidComposition();
        fixture.WriteBuildCs("Core", "PrivateDependencyModuleNames.Add(\"ForbiddenLeaf\");");
        fixture.WriteBuildCs("ForbiddenLeaf", "");

        var result = Check(fixture);

        CollectionAssert.AreEqual(new[] { "Core has forbidden simulation dependency ForbiddenLeaf" }, result.Diagnostics.ToArray());
    }

    [TestMethod]
    public void Check_reports_missing_simulation_module_and_continues_other_checks()
    {
        using var fixture = new ArchitectureCheckFixture();
        fixture.WriteBuildCs("SpaceGamePresentation", "PublicDependencyModuleNames.Add(\"SpaceGame\");");
        fixture.WriteBuildCs("SpaceGame", "PublicDependencyModuleNames.Add(\"SpaceGameSimulation\");");

        var result = Check(fixture);

        CollectionAssert.AreEqual(
            new[]
            {
                "Presentation must depend on Simulation, never on SpaceGame",
                "SpaceGame must compose both layers",
                "SpaceGameSimulation runtime module is missing",
            },
            result.Diagnostics.ToArray());
    }

    [DataTestMethod]
    [DataRow("FLevelSimulation* simulation;")]
    [DataRow("FLevelSimulation& simulation;")]
    [DataRow("Simulation* simulation;")]
    [DataRow("Simulation& simulation;")]
    public void Check_rejects_presentation_mutable_simulation_pointers_and_references(string contents)
    {
        using var fixture = new ArchitectureCheckFixture();
        fixture.WriteValidComposition();
        fixture.WritePresentationSource("Private/Presentation.cpp", contents);

        var result = Check(fixture);

        CollectionAssert.AreEqual(
            new[] { Path.Combine("Plugins", "SpaceGame", "Source", "SpaceGamePresentation", "Private", "Presentation.cpp") + ": mutable simulation access" },
            result.Diagnostics.ToArray());
    }

    [TestMethod]
    public void Check_rejects_presentation_const_cast()
    {
        using var fixture = new ArchitectureCheckFixture();
        fixture.WriteValidComposition();
        fixture.WritePresentationSource("Public/Presentation.h", "const_cast<FLevelSimulation&>(simulation);");

        var result = Check(fixture);

        CollectionAssert.AreEqual(
            new[] { Path.Combine("Plugins", "SpaceGame", "Source", "SpaceGamePresentation", "Public", "Presentation.h") + ": mutable simulation access" },
            result.Diagnostics.ToArray());
    }

    [TestMethod]
    public void Check_rejects_invalid_presentation_composition()
    {
        using var fixture = new ArchitectureCheckFixture();
        fixture.WriteBuildCs("SpaceGameSimulation", "PublicDependencyModuleNames.Add(\"Core\");");
        fixture.WriteBuildCs("SpaceGamePresentation", "PublicDependencyModuleNames.Add(\"SpaceGame\");");
        fixture.WriteBuildCs("SpaceGame", "PublicDependencyModuleNames.AddRange(new string[] { \"SpaceGameSimulation\", \"SpaceGamePresentation\" });");

        var result = Check(fixture);

        CollectionAssert.AreEqual(new[] { "Presentation must depend on Simulation, never on SpaceGame" }, result.Diagnostics.ToArray());
    }

    [DataTestMethod]
    [DataRow("PublicDependencyModuleNames.Add(\"SpaceGameSimulation\");")]
    [DataRow("PublicDependencyModuleNames.Add(\"SpaceGamePresentation\");")]
    public void Check_rejects_incomplete_space_game_composition(string composition)
    {
        using var fixture = new ArchitectureCheckFixture();
        fixture.WriteBuildCs("SpaceGameSimulation", "PublicDependencyModuleNames.Add(\"Core\");");
        fixture.WriteBuildCs("SpaceGamePresentation", "PublicDependencyModuleNames.Add(\"SpaceGameSimulation\");");
        fixture.WriteBuildCs("SpaceGame", composition);

        var result = Check(fixture);

        CollectionAssert.AreEqual(new[] { "SpaceGame must compose both layers" }, result.Diagnostics.ToArray());
    }

    [TestMethod]
    public void Check_reads_utf8_bom_build_files_and_collects_multiple_failures_in_order()
    {
        using var fixture = new ArchitectureCheckFixture();
        fixture.WriteBuildCs("SpaceGameSimulation", "PublicDependencyModuleNames.Add(\"Forbidden\");", bom: true);
        fixture.WriteBuildCs("SpaceGamePresentation", "PrivateDependencyModuleNames.Add(\"SpaceGame\");");
        fixture.WriteBuildCs("SpaceGame", "");
        fixture.WriteSimulationSource("Private/Forbidden.inl", "#include <SpaceGame/Anything.h>");
        fixture.WritePresentationSource("Private/Mutable.cpp", "Simulation& simulation;");

        var result = Check(fixture);

        CollectionAssert.AreEqual(
            new[]
            {
                Path.Combine("Plugins", "SpaceGame", "Source", "SpaceGamePresentation", "Private", "Mutable.cpp") + ": mutable simulation access",
                Path.Combine("Plugins", "SpaceGame", "Source", "SpaceGameSimulation", "Private", "Forbidden.inl") + ":1: forbidden layer reference",
                "Presentation must depend on Simulation, never on SpaceGame",
                "SpaceGame must compose both layers",
                "SpaceGameSimulation has forbidden simulation dependency Forbidden",
            },
            result.Diagnostics.ToArray());
    }

    [TestMethod]
    public void TryParse_accepts_only_an_explicit_root()
    {
        Assert.IsTrue(Program.TryParse(["--root", "repository root"], out var request));
        Assert.AreEqual("repository root", request!.RootPath);
        Assert.IsFalse(Program.TryParse([], out _));
        Assert.IsFalse(Program.TryParse(["--root"], out _));
    }

    private static ArchitectureCheckResult Check(ArchitectureCheckFixture fixture)
    {
        return new SpaceGameLayerChecker().Check(new ArchitectureCheckRequest(fixture.Root));
    }
}
