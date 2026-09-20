using System.Text;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace ArchitectureChecks.Tests;

[TestClass]
public sealed class ModuleMigrationParserTests
{
    [TestMethod]
    public void Reflected_type_parser_handles_api_macros_multiline_macros_and_comments()
    {
        var types = ReflectedTypeParser.Parse(
            """
            // UCLASS() class AComment {};
            UCLASS(
                meta=(DisplayName="Ship")
            )
            class SANDBOX_API AShip final {};

            UINTERFACE()
            class SANDBOX_API UTargetable {};
            class SANDBOX_API ITargetable {};

            USTRUCT(BlueprintType)
            struct SANDBOX_API FShipData {};
            UENUM(BlueprintType)
            enum class EShipState : uint8 { Idle };
            """
        );

        CollectionAssert.AreEqual(
            new[]
            {
                new ReflectedType(ReflectedTypeKind.Class, "AShip"),
                new ReflectedType(ReflectedTypeKind.Class, "UTargetable"),
                new ReflectedType(ReflectedTypeKind.Struct, "FShipData"),
                new ReflectedType(ReflectedTypeKind.Enum, "EShipState"),
            },
            types.ToArray());
    }

    [TestMethod]
    public void Core_redirect_parser_matches_semantic_fields_without_relying_on_field_order()
    {
        var redirects = new CoreRedirectParser().Parse(
            """
            +ClassRedirects=( NewName = "/Script/ShooterGame.Ship", OldName = "/Script/Sandbox.Ship" )
            +StructRedirects=(OldName="/Script/Sandbox.Loadout",NewName="/Script/ShooterGame.Loadout")
            +EnumRedirects=(OldName="/Script/Sandbox.EShipState",NewName="/Script/ShooterGame.EShipState")
            +ClassRedirects=(OldName="/Script/Sandbox.Before",MatchSubstring=true,NewName="/Script/ShooterGame.Before")
            +ClassRedirects=(MatchSubstring=false,NewName="/Script/ShooterGame.After",OldName="/Script/Sandbox.After")
            +ClassRedirects=(OldName="/Script/Sandbox.Trailing",NewName="/Script/ShooterGame.Trailing",MatchSubstring=true)
            """
        );

        Assert.IsTrue(redirects.Contains(new CoreRedirect(ReflectedTypeKind.Class, "/Script/Sandbox.Ship", "/Script/ShooterGame.Ship")));
        Assert.IsTrue(redirects.Contains(new CoreRedirect(ReflectedTypeKind.Struct, "/Script/Sandbox.Loadout", "/Script/ShooterGame.Loadout")));
        Assert.IsTrue(redirects.Contains(new CoreRedirect(ReflectedTypeKind.Enum, "/Script/Sandbox.EShipState", "/Script/ShooterGame.EShipState")));
        Assert.IsTrue(redirects.Contains(new CoreRedirect(ReflectedTypeKind.Class, "/Script/Sandbox.Before", "/Script/ShooterGame.Before")));
        Assert.IsTrue(redirects.Contains(new CoreRedirect(ReflectedTypeKind.Class, "/Script/Sandbox.After", "/Script/ShooterGame.After")));
        Assert.IsTrue(redirects.Contains(new CoreRedirect(ReflectedTypeKind.Class, "/Script/Sandbox.Trailing", "/Script/ShooterGame.Trailing")));
    }

    [TestMethod]
    public void Core_redirect_parser_ignores_malformed_entries_and_preserves_escaped_quoted_values()
    {
        var redirects = new CoreRedirectParser().Parse(
            """
            +ClassRedirects=(OldName="/Script/Sandbox.MissingNew")
            +ClassRedirects=(OldName="/Script/Sandbox.Unterminated,NewName="/Script/ShooterGame.Unterminated")
            +ClassRedirects=(OldName="/Script/Sandbox.Say\"Hi",NewName="/Script/ShooterGame.Say\"Hi")
            """
        );

        Assert.AreEqual(1, redirects.Count);
        Assert.IsTrue(redirects.Contains(new CoreRedirect(
            ReflectedTypeKind.Class,
            "/Script/Sandbox.Say\"Hi",
            "/Script/ShooterGame.Say\"Hi")));
    }

    [TestMethod]
    public void Rename_parser_handles_nul_delimited_paths_with_spaces()
    {
        var output = Encoding.UTF8.GetBytes("R100\0Source/Sandbox/Space Ship.h\0Plugins/ShooterGame/Source/ShooterGame/Public/ShooterGame/Space Ship.h\0");

        var renames = ModuleMigrationGit.ParseRenameRecords(output, "Sandbox", ["ShooterGame"]);

        Assert.AreEqual(1, renames.Count);
        Assert.AreEqual("ShooterGame", renames[0].DestinationModule);
        Assert.AreEqual("Source/Sandbox/Space Ship.h", renames[0].SourcePath);
    }

    [TestMethod]
    public void Rename_parser_rejects_malformed_nul_delimited_output()
    {
        var output = Encoding.UTF8.GetBytes("R100\0Source/Sandbox/Ship.h\0");

        Assert.ThrowsException<ModuleMigrationException>(
            () => ModuleMigrationGit.ParseRenameRecords(output, "Sandbox", ["ShooterGame"]));
    }

    [TestMethod]
    public async Task Git_query_uses_argument_list_paths_and_nul_delimited_name_status_output()
    {
        var runner = new RecordingProcessRunner(new ProcessResult(0, [], string.Empty));
        var git = new ModuleMigrationGit(runner);

        var renames = await git.GetRenamesAsync(
            "repository root",
            new ModuleMigrationRequest("repository root", "base rev", "OldModule", ["FirstPlugin", "SecondPlugin"]),
            CancellationToken.None);

        Assert.AreEqual(0, renames.Count);
        CollectionAssert.AreEqual(
            new[]
            {
                "-C", Path.GetFullPath("repository root"),
                "-c", "core.safecrlf=false",
                "diff", "--name-status", "-z", "--find-renames=20%", "base rev", "--",
                "Source/OldModule", "Plugins/FirstPlugin", "Plugins/SecondPlugin",
            },
            runner.Request!.Arguments.ToArray());
    }

    [TestMethod]
    public void Module_migration_cli_appends_explicit_plugin_modules_to_defaults()
    {
        Assert.IsTrue(Program.TryParseModuleMigration(["module-migration", "--root", "repo"], out var defaults));
        CollectionAssert.AreEqual(new[] { "ShooterGame", "SandboxGameShared" }, defaults!.PluginModules.ToArray());
        Assert.AreEqual("HEAD", defaults.Baseline);
        Assert.AreEqual("Sandbox", defaults.OldModule);

        Assert.IsTrue(
            Program.TryParseModuleMigration(
                ["module-migration", "--root", "repo", "--baseline", "abc^", "--old-module", "Old", "--plugin-module", "One", "--plugin-module", "Two"],
                out var explicit_modules));
        CollectionAssert.AreEqual(new[] { "ShooterGame", "SandboxGameShared", "One", "Two" }, explicit_modules!.PluginModules.ToArray());
        Assert.AreEqual("abc^", explicit_modules.Baseline);
        Assert.AreEqual("Old", explicit_modules.OldModule);

        Assert.IsTrue(
            Program.TryParseModuleMigration(
                ["module-migration", "--root", "repo", "--plugin-module", "One", "--plugin-module", "One"],
                out var duplicate_modules));
        CollectionAssert.AreEqual(new[] { "ShooterGame", "SandboxGameShared", "One" }, duplicate_modules!.PluginModules.ToArray());

        Assert.IsTrue(
            Program.TryParseModuleMigration(
                ["module-migration", "--root", "repo", "--plugin-module", "ShooterGame"],
                out var duplicate_default));
        CollectionAssert.AreEqual(new[] { "ShooterGame", "SandboxGameShared" }, duplicate_default!.PluginModules.ToArray());

        Assert.IsFalse(Program.TryParseModuleMigration(["module-migration", "--plugin-module", "One"], out _));
        Assert.IsFalse(Program.TryParseModuleMigration(["module-migration", "--root", "repo", "--old-module", "not/a/module"], out _));
        Assert.IsFalse(Program.TryParseModuleMigration(["module-migration", "--root", "repo", "--plugin-module", "not/a/module"], out _));
    }

    [TestMethod]
    public void Repository_path_containment_allows_only_in_repository_paths()
    {
        using var fixture = new ArchitectureCheckFixture();

        Assert.AreEqual(
            Path.Combine(fixture.Root, "Source", "Thing.h"),
            ModuleMigrationChecker.ToFullPath(fixture.Root, "Source/Thing.h"));
        Assert.ThrowsException<ModuleMigrationException>(
            () => ModuleMigrationChecker.ToFullPath(fixture.Root, ".."));
        Assert.ThrowsException<ModuleMigrationException>(
            () => ModuleMigrationChecker.ToFullPath(fixture.Root, "nested/../../Outside.h"));
        Assert.ThrowsException<ModuleMigrationException>(
            () => ModuleMigrationChecker.ToFullPath(fixture.Root, Path.Combine(fixture.Root, "Rooted.h")));
        Assert.AreEqual(
            Path.Combine(fixture.Root, "..valid", "Thing.h"),
            ModuleMigrationChecker.ToFullPath(fixture.Root, "..valid/Thing.h"));
    }

    [TestMethod]
    [DoNotParallelize]
    public void Module_migration_help_returns_success_and_invalid_arguments_remain_errors()
    {
        var help = RunProgram(["module-migration", "--help"]);
        Assert.AreEqual(0, help.ExitCode);
        StringAssert.Contains(help.StandardOutput, "Usage: ArchitectureChecks module-migration");
        StringAssert.Contains(help.StandardOutput, "old module Sandbox");
        StringAssert.Contains(help.StandardOutput, "ShooterGame and SandboxGameShared");
        StringAssert.Contains(help.StandardOutput, "adds to the default plugin modules");
        StringAssert.Contains(help.StandardOutput, "advisory");

        var short_help = RunProgram(["module-migration", "-h"]);
        Assert.AreEqual(0, short_help.ExitCode);

        var invalid = RunProgram(["module-migration"]);
        Assert.AreEqual(2, invalid.ExitCode);
        StringAssert.Contains(invalid.StandardError, "ArchitectureChecks module-migration --help");
    }

    private static ProgramResult RunProgram(IReadOnlyList<string> arguments)
    {
        var standard_output = new StringWriter();
        var standard_error = new StringWriter();
        var original_output = Console.Out;
        var original_error = Console.Error;
        try
        {
            Console.SetOut(standard_output);
            Console.SetError(standard_error);
            var exit_code = Program.Main(arguments.ToArray());
            return new ProgramResult(exit_code, standard_output.ToString(), standard_error.ToString());
        }
        finally
        {
            Console.SetOut(original_output);
            Console.SetError(original_error);
        }
    }

    private sealed record ProgramResult(int ExitCode, string StandardOutput, string StandardError);

    private sealed class RecordingProcessRunner(ProcessResult result) : IProcessRunner
    {
        public ProcessRequest? Request { get; private set; }

        public Task<ProcessResult> RunAsync(ProcessRequest request, CancellationToken cancellation_token)
        {
            Request = request;
            return Task.FromResult(result);
        }
    }
}
