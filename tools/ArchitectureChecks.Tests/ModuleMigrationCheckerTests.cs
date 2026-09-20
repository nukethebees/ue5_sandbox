using System.Text;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace ArchitectureChecks.Tests;

[TestClass]
public sealed class ModuleMigrationCheckerTests
{
    [TestMethod]
    public async Task Check_reports_a_class_move_with_a_matching_redirect()
    {
        using var fixture = new ModuleMigrationFixture();
        var baseline = CreateMigrationBaseline(fixture, "actors/Ship.h", "UCLASS()\nclass SANDBOX_API AShip {};");
        fixture.Move("Source/Sandbox/actors/Ship.h", Destination("ShooterGame", "actors/Ship.h"));
        fixture.WriteFile("Config/DefaultEngine.ini", Redirect(ReflectedTypeKind.Class, "Ship", "ShooterGame"));
        fixture.Commit("Move Ship");

        var result = await CheckAsync(fixture, baseline, "ShooterGame");

        Assert.AreEqual(1, result.RenamedFileCount);
        Assert.AreEqual(1, result.MovedTypes.Count);
        Assert.AreEqual("AShip", result.MovedTypes[0].Type.CppName);
        Assert.IsTrue(result.MovedTypes[0].HasRedirect);
    }

    [TestMethod]
    public async Task Check_reports_a_missing_class_redirect()
    {
        using var fixture = new ModuleMigrationFixture();
        var baseline = CreateMigrationBaseline(fixture, "actors/Ship.h", "UCLASS() class SANDBOX_API AShip {};");
        fixture.Move("Source/Sandbox/actors/Ship.h", Destination("ShooterGame", "actors/Ship.h"));
        fixture.Commit("Move Ship");

        var result = await CheckAsync(fixture, baseline, "ShooterGame");

        Assert.AreEqual(1, result.MovedTypes.Count);
        Assert.IsFalse(result.MovedTypes[0].HasRedirect);
        Assert.AreEqual("/Script/Sandbox.Ship", result.MovedTypes[0].OldObjectPath);
    }

    [DataTestMethod]
    [DataRow("USTRUCT(BlueprintType) struct SANDBOX_API FLoadout {};", ReflectedTypeKind.Struct, "Loadout")]
    [DataRow("UENUM(BlueprintType) enum class EDisposition : uint8 { Friendly };", ReflectedTypeKind.Enum, "EDisposition")]
    public async Task Check_reports_struct_and_enum_moves(string declaration, ReflectedTypeKind kind, string reflection_name)
    {
        using var fixture = new ModuleMigrationFixture();
        var baseline = CreateMigrationBaseline(fixture, "data/Types.h", declaration);
        fixture.Move("Source/Sandbox/data/Types.h", Destination("SandboxGameShared", "data/Types.h"));
        fixture.WriteFile("Config/DefaultEngine.ini", Redirect(kind, reflection_name, "SandboxGameShared"));
        fixture.Commit("Move Types");

        var result = await CheckAsync(fixture, baseline, "SandboxGameShared");

        Assert.AreEqual(1, result.MovedTypes.Count);
        Assert.AreEqual(kind, result.MovedTypes[0].Type.Kind);
        Assert.IsTrue(result.MovedTypes[0].HasRedirect);
    }

    [TestMethod]
    public async Task Check_handles_uinterface_without_treating_the_native_interface_as_reflected()
    {
        using var fixture = new ModuleMigrationFixture();
        const string declaration = """
            UINTERFACE(BlueprintType)
            class SANDBOX_API UTargetable : public UInterface {};
            class SANDBOX_API ITargetable { };
            """;
        var baseline = CreateMigrationBaseline(fixture, "interfaces/Targetable.h", declaration);
        fixture.Move("Source/Sandbox/interfaces/Targetable.h", Destination("ShooterGame", "interfaces/Targetable.h"));
        fixture.WriteFile("Config/DefaultEngine.ini", Redirect(ReflectedTypeKind.Class, "Targetable", "ShooterGame"));
        fixture.Commit("Move interface");

        var result = await CheckAsync(fixture, baseline, "ShooterGame");

        Assert.AreEqual(1, result.MovedTypes.Count);
        Assert.AreEqual("UTargetable", result.MovedTypes[0].Type.CppName);
        Assert.IsTrue(result.MovedTypes[0].HasRedirect);
    }

    [TestMethod]
    public async Task Check_requires_the_same_reflected_type_in_the_destination_header()
    {
        using var fixture = new ModuleMigrationFixture();
        const string source = """
            #pragma once
            // Stable declaration context.
            // Stable declaration context.
            // Stable declaration context.
            UCLASS() class SANDBOX_API AShip {};
            """;
        var baseline = CreateMigrationBaseline(fixture, "actors/Ship.h", source);
        fixture.Move("Source/Sandbox/actors/Ship.h", Destination("ShooterGame", "actors/Ship.h"));
        fixture.WriteFile(Destination("ShooterGame", "actors/Ship.h"), source.Replace("AShip", "AOtherShip", StringComparison.Ordinal));
        fixture.Commit("Rename unrelated header");

        var result = await CheckAsync(fixture, baseline, "ShooterGame");

        Assert.AreEqual(1, result.RenamedFileCount);
        Assert.AreEqual(0, result.MovedTypes.Count);
    }

    [TestMethod]
    public async Task Check_reports_multiple_reflected_types_from_one_header()
    {
        using var fixture = new ModuleMigrationFixture();
        const string declarations = """
            UCLASS(meta=(DisplayName="Ship"))
            class SANDBOX_API AShip {};
            USTRUCT()
            struct SANDBOX_API FShipData {};
            UENUM()
            enum class EShipState : uint8 { Idle };
            """;
        var baseline = CreateMigrationBaseline(fixture, "actors/Ship.h", declarations);
        fixture.Move("Source/Sandbox/actors/Ship.h", Destination("ShooterGame", "actors/Ship.h"));
        fixture.WriteFile(
            "Config/DefaultEngine.ini",
            string.Join('\n',
                Redirect(ReflectedTypeKind.Class, "Ship", "ShooterGame"),
                Redirect(ReflectedTypeKind.Struct, "ShipData", "ShooterGame"),
                Redirect(ReflectedTypeKind.Enum, "EShipState", "ShooterGame")));
        fixture.Commit("Move types");

        var result = await CheckAsync(fixture, baseline, "ShooterGame");

        Assert.AreEqual(3, result.MovedTypes.Count);
        Assert.IsTrue(result.MovedTypes.All(finding => finding.HasRedirect));
    }

    [TestMethod]
    public async Task Check_reports_a_clean_migration_without_missing_redirects_or_include_findings()
    {
        using var fixture = new ModuleMigrationFixture();
        var baseline = CreateMigrationBaseline(fixture, "actors/Clean.h", "UCLASS() class SANDBOX_API AClean {};");
        fixture.Move("Source/Sandbox/actors/Clean.h", Destination("ShooterGame", "actors/Clean.h"));
        fixture.WriteFile("Config/DefaultEngine.ini", Redirect(ReflectedTypeKind.Class, "Clean", "ShooterGame"));
        fixture.Commit("Clean migration");

        var result = await CheckAsync(fixture, baseline, "ShooterGame");

        Assert.IsTrue(result.MovedTypes.All(finding => finding.HasRedirect));
        Assert.AreEqual(0, result.StaleIncludes.Count);
        Assert.AreEqual(0, result.RelativeIncludes.Count);
        CollectionAssert.AreEqual(
            new[] { "Config/DefaultEngine.ini" },
            result.OldModuleReferences.Select(finding => finding.Path).ToArray());
    }

    [TestMethod]
    public async Task Check_identifies_destination_modules_from_the_selected_plugin_set()
    {
        using var fixture = new ModuleMigrationFixture();
        CreateMigrationBaseline(fixture, "actors/One.h", "UCLASS() class SANDBOX_API AOne {};");
        fixture.WriteFile("Source/Sandbox/actors/Two.h", "UCLASS() class SANDBOX_API ATwo {};");
        var baseline = fixture.Commit("Add second type");
        fixture.Move("Source/Sandbox/actors/One.h", Destination("ShooterGame", "actors/One.h"));
        fixture.Move("Source/Sandbox/actors/Two.h", Destination("SandboxGameShared", "actors/Two.h"));
        fixture.Commit("Split destination modules");

        var result = await CheckAsync(fixture, baseline, "ShooterGame", "SandboxGameShared");

        CollectionAssert.AreEquivalent(
            new[] { "ShooterGame", "SandboxGameShared" },
            result.MovedTypes.Select(finding => finding.DestinationModule).ToArray());
    }

    [TestMethod]
    public async Task Check_finds_old_script_references_in_text_and_binary_like_files()
    {
        using var fixture = new ModuleMigrationFixture();
        fixture.WriteFile("Config/DefaultEngine.ini", "Value=/Script/Sandbox.ConfigType");
        fixture.WriteBytes("Content/Test.uasset", Encoding.UTF8.GetBytes("\0/Script/Sandbox.AssetType\0"));
        fixture.WriteFile("Plugins/Fixture/Reference.txt", "/Script/Sandbox.PluginType");
        var baseline = fixture.Commit("References");

        var result = await CheckAsync(fixture, baseline, "ShooterGame");

        CollectionAssert.AreEquivalent(
            new[]
            {
                "Config/DefaultEngine.ini",
                "Content/Test.uasset",
                "Plugins/Fixture/Reference.txt",
            },
            result.OldModuleReferences.Select(finding => finding.Path).ToArray());
    }

    [TestMethod]
    public async Task Check_finds_stale_includes_only_when_a_plugin_counterpart_exists()
    {
        using var fixture = new ModuleMigrationFixture();
        fixture.WriteFile(Destination("ShooterGame", "combat/Weapon.h"), "#pragma once");
        fixture.WriteFile("Source/Sandbox/Consumer.cpp", "#include <Sandbox/combat/Weapon.h>\n#include <Sandbox/combat/Absent.h>");
        var baseline = fixture.Commit("Includes");

        var result = await CheckAsync(fixture, baseline, "ShooterGame");

        Assert.AreEqual(1, result.StaleIncludes.Count);
        Assert.AreEqual("combat/Weapon.h", result.StaleIncludes[0].IncludePath);
        CollectionAssert.AreEqual(new[] { "ShooterGame" }, result.StaleIncludes[0].CounterpartModules.ToArray());
    }

    [TestMethod]
    public async Task Check_reports_all_duplicate_plugin_header_counterparts()
    {
        using var fixture = new ModuleMigrationFixture();
        fixture.WriteFile(Destination("ShooterGame", "combat/Weapon.h"), "#pragma once");
        fixture.WriteFile(Destination("SandboxGameShared", "combat/Weapon.h"), "#pragma once");
        fixture.WriteFile("Source/Sandbox/Consumer.cpp", "#include \"Sandbox/combat/Weapon.h\"");
        var baseline = fixture.Commit("Includes");

        var result = await CheckAsync(fixture, baseline, "ShooterGame", "SandboxGameShared");

        Assert.AreEqual(1, result.StaleIncludes.Count);
        CollectionAssert.AreEqual(
            new[] { "SandboxGameShared", "ShooterGame" },
            result.StaleIncludes[0].CounterpartModules.ToArray());
    }

    [TestMethod]
    public async Task Check_finds_quoted_relative_includes_only_in_selected_plugin_implementations()
    {
        using var fixture = new ModuleMigrationFixture();
        fixture.WriteFile("Plugins/ShooterGame/Source/ShooterGame/Private/Thing.cpp", "#include \"Thing.h\"\n#include \"nested/Thing.h\"");
        fixture.WriteFile("Plugins/ShooterGame/Source/ShooterGame/Public/Thing.h", "#include \"Thing.h\"");
        var baseline = fixture.Commit("Relative includes");

        var result = await CheckAsync(fixture, baseline, "ShooterGame");

        Assert.AreEqual(1, result.RelativeIncludes.Count);
        Assert.AreEqual("Thing.h", result.RelativeIncludes[0].IncludeText);
    }

    [TestMethod]
    public async Task Check_reports_no_renames_for_a_baseline_without_migration()
    {
        using var fixture = new ModuleMigrationFixture();
        fixture.WriteFile("Source/Sandbox/Unmoved.h", "UCLASS() class SANDBOX_API AUnmoved {};");
        var baseline = fixture.Commit("No migration");

        var result = await CheckAsync(fixture, baseline, "ShooterGame");

        Assert.AreEqual(0, result.RenamedFileCount);
        Assert.AreEqual(0, result.MovedTypes.Count);
    }

    [TestMethod]
    public async Task Check_accepts_spaces_in_renamed_paths()
    {
        using var fixture = new ModuleMigrationFixture();
        var baseline = CreateMigrationBaseline(fixture, "actors/Space Ship.h", "UCLASS() class SANDBOX_API ASpaceShip {};");
        fixture.Move("Source/Sandbox/actors/Space Ship.h", Destination("ShooterGame", "actors/Space Ship.h"));
        fixture.Commit("Move space path");

        var result = await CheckAsync(fixture, baseline, "ShooterGame");

        Assert.AreEqual(1, result.RenamedFileCount);
        Assert.AreEqual("ASpaceShip", result.MovedTypes[0].Type.CppName);
    }

    private static string CreateMigrationBaseline(ModuleMigrationFixture fixture, string relative_header, string contents)
    {
        fixture.WriteFile(Path.Combine("Source", "Sandbox", relative_header), contents);
        return fixture.Commit("Baseline");
    }

    private static string Destination(string module, string relative_header)
    {
        return Path.Combine("Plugins", module, "Source", module, "Public", module, relative_header);
    }

    private static string Redirect(ReflectedTypeKind kind, string reflection_name, string module)
    {
        return $"+{kind}Redirects=(NewName=\"/Script/{module}.{reflection_name}\", OldName=\"/Script/Sandbox.{reflection_name}\")";
    }

    private static Task<ModuleMigrationResult> CheckAsync(ModuleMigrationFixture fixture, string baseline, params string[] plugin_modules)
    {
        return new ModuleMigrationChecker().CheckAsync(
            new ModuleMigrationRequest(fixture.Root, baseline, "Sandbox", plugin_modules),
            CancellationToken.None);
    }
}
