using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace GamePackageTools.Tests;

[TestClass]
public sealed class PackageVerifierTests
{
    [TestMethod]
    public async Task VerifyAsync_accepts_a_complete_development_package_and_writes_inventory_artifacts()
    {
        using var fixture = new PackageToolFixture();
        var runner = fixture.CreateRunner();
        var output = new StringWriter();

        await new PackageVerifier(runner, output).VerifyAsync(fixture.CreateRequest());

        Assert.AreEqual(1, runner.PakFiles.Count);
        Assert.IsTrue(File.Exists(Path.Combine(fixture.VerificationDirectory, "pak-files.txt")));
        Assert.IsTrue(File.Exists(Path.Combine(fixture.VerificationDirectory, "iostore.csv")));
        StringAssert.Contains(output.ToString(), "Verified 1 level scripts");
    }

    [TestMethod]
    public void GetGameBinaryName_uses_development_and_shipping_conventions()
    {
        Assert.AreEqual("Sandbox.exe", PackageVerifier.GetGameBinaryName(PackageConfiguration.Development));
        Assert.AreEqual("Sandbox-Win64-Shipping.exe", PackageVerifier.GetGameBinaryName(PackageConfiguration.Shipping));
    }

    [TestMethod]
    public async Task VerifyAsync_reports_missing_required_files()
    {
        using var fixture = new PackageToolFixture();
        var missing = Path.Combine(fixture.PackageRoot, "Sandbox.exe");
        File.Delete(missing);

        var exception = await Assert.ThrowsExceptionAsync<PackageVerificationException>(
            () => new PackageVerifier(fixture.CreateRunner(), TextWriter.Null).VerifyAsync(fixture.CreateRequest()));

        StringAssert.Contains(exception.Message, missing);
    }

    [TestMethod]
    public async Task VerifyAsync_reports_missing_pak_and_utoc_containers()
    {
        using var missingPak = new PackageToolFixture();
        File.Delete(Path.Combine(missingPak.PakDirectory, "Sandbox-Windows.pak"));
        var pakException = await Assert.ThrowsExceptionAsync<PackageVerificationException>(
            () => new PackageVerifier(missingPak.CreateRunner(), TextWriter.Null).VerifyAsync(missingPak.CreateRequest()));
        StringAssert.Contains(pakException.Message, "No pak files");

        using var missingUtoc = new PackageToolFixture();
        File.Delete(Path.Combine(missingUtoc.PakDirectory, "Sandbox-Windows.utoc"));
        var utocException = await Assert.ThrowsExceptionAsync<PackageVerificationException>(
            () => new PackageVerifier(missingUtoc.CreateRunner(), TextWriter.Null).VerifyAsync(missingUtoc.CreateRequest()));
        StringAssert.Contains(utocException.Message, "No IoStore containers");
    }

    [TestMethod]
    public async Task VerifyAsync_accepts_required_packages_found_by_staged_path()
    {
        using var fixture = new PackageToolFixture();
        var runner = fixture.CreateRunner();
        runner.IoStoreContents = fixture.BuildInventory()
            .Replace("/SpaceGame/Levels/MainMenu", "../../../Sandbox/Plugins/SpaceGame/Content/Levels/MainMenu", StringComparison.Ordinal)
            .Replace("/SpaceGame/Levels/GameRuntime", "../../../Sandbox/Plugins/SpaceGame/Content/Levels/GameRuntime", StringComparison.Ordinal)
            .Replace("/Game/UI/DA_ui_data", "../../../Sandbox/Content/UI/DA_ui_data", StringComparison.Ordinal)
            .Replace("/SpaceGame/UI/RequiredUi", "../../../Sandbox/Plugins/SpaceGame/Content/UI/RequiredUi", StringComparison.Ordinal)
            .Replace("/SpaceGame/Input/RequiredInput", "../../../Sandbox/Plugins/SpaceGame/Content/Input/RequiredInput", StringComparison.Ordinal)
            .Replace("/SandboxShaders/GpuStarfield/RequiredStarfield", "../../../Sandbox/Plugins/SandboxShaders/Content/GpuStarfield/RequiredStarfield", StringComparison.Ordinal)
            .Replace("/SandboxShaders/CelestialBackdrop/RequiredBackdrop", "../../../Sandbox/Plugins/SandboxShaders/Content/CelestialBackdrop/RequiredBackdrop", StringComparison.Ordinal);

        await new PackageVerifier(runner, TextWriter.Null).VerifyAsync(fixture.CreateRequest());
    }

    [TestMethod]
    public async Task VerifyAsync_reports_missing_required_packages()
    {
        using var fixture = new PackageToolFixture();
        var runner = fixture.CreateRunner();
        runner.IoStoreContents = runner.IoStoreContents.Replace("/Game/UI/DA_ui_data", string.Empty, StringComparison.Ordinal);

        var exception = await Assert.ThrowsExceptionAsync<PackageVerificationException>(
            () => new PackageVerifier(runner, TextWriter.Null).VerifyAsync(fixture.CreateRequest()));

        StringAssert.Contains(exception.Message, "/Game/UI/DA_ui_data");
    }

    [TestMethod]
    public async Task VerifyAsync_reports_missing_level_scripts_using_relative_paths()
    {
        using var fixture = new PackageToolFixture();
        var runner = fixture.CreateRunner();
        runner.IoStoreContents = runner.IoStoreContents.Replace("LevelScripts/Nested/Scenario.scm", string.Empty, StringComparison.Ordinal);

        var exception = await Assert.ThrowsExceptionAsync<PackageVerificationException>(
            () => new PackageVerifier(runner, TextWriter.Null).VerifyAsync(fixture.CreateRequest()));

        StringAssert.Contains(exception.Message, "Nested/Scenario.scm");
    }

    [TestMethod]
    public async Task VerifyAsync_requires_both_required_maps()
    {
        using var fixture = new PackageToolFixture();
        var runner = fixture.CreateRunner();
        runner.IoStoreContents = runner.IoStoreContents.Replace("/SpaceGame/Levels/GameRuntime", string.Empty, StringComparison.Ordinal);

        var exception = await Assert.ThrowsExceptionAsync<PackageVerificationException>(
            () => new PackageVerifier(runner, TextWriter.Null).VerifyAsync(fixture.CreateRequest()));

        StringAssert.Contains(exception.Message, "/SpaceGame/Levels/GameRuntime");
    }

    [TestMethod]
    public async Task VerifyAsync_rejects_unexpected_project_maps()
    {
        using var fixture = new PackageToolFixture();
        fixture.WriteProjectFile("Content/Levels/Unexpected.umap");
        var runner = fixture.CreateRunner();
        runner.IoStoreContents += "\n/Game/Levels/Unexpected";

        var exception = await Assert.ThrowsExceptionAsync<PackageVerificationException>(
            () => new PackageVerifier(runner, TextWriter.Null).VerifyAsync(fixture.CreateRequest()));

        StringAssert.Contains(exception.Message, "Unexpected project map was packaged: /Game/Levels/Unexpected");
    }

    [TestMethod]
    public async Task VerifyAsync_requires_assets_from_each_required_directory()
    {
        using var fixture = new PackageToolFixture();
        var runner = fixture.CreateRunner();
        runner.IoStoreContents = runner.IoStoreContents.Replace("/SandboxShaders/GpuStarfield/RequiredStarfield", string.Empty, StringComparison.Ordinal);

        var exception = await Assert.ThrowsExceptionAsync<PackageVerificationException>(
            () => new PackageVerifier(runner, TextWriter.Null).VerifyAsync(fixture.CreateRequest()));

        StringAssert.Contains(exception.Message, "/SandboxShaders/GpuStarfield/RequiredStarfield");
    }

    [TestMethod]
    public async Task VerifyAsync_allows_missing_optional_audio_directory_and_checks_it_when_present()
    {
        using var fixture = new PackageToolFixture();
        await new PackageVerifier(fixture.CreateRunner(), TextWriter.Null).VerifyAsync(fixture.CreateRequest());

        fixture.WriteProjectFile("Plugins/SpaceGame/Content/Audio/Generated/GeneratedAudio.uasset");
        var runner = fixture.CreateRunner();
        var exception = await Assert.ThrowsExceptionAsync<PackageVerificationException>(
            () => new PackageVerifier(runner, TextWriter.Null).VerifyAsync(fixture.CreateRequest()));
        StringAssert.Contains(exception.Message, "/SpaceGame/Audio/Generated/GeneratedAudio");

        runner = fixture.CreateRunner();
        runner.IoStoreContents += "\n/SpaceGame/Audio/Generated/GeneratedAudio";
        await new PackageVerifier(runner, TextWriter.Null).VerifyAsync(fixture.CreateRequest());
    }

    [TestMethod]
    public async Task VerifyAsync_removes_stale_iostore_csv_before_requiring_new_output()
    {
        using var fixture = new PackageToolFixture();
        Directory.CreateDirectory(fixture.VerificationDirectory);
        var csvPath = Path.Combine(fixture.VerificationDirectory, "iostore.csv");
        File.WriteAllText(csvPath, "stale inventory");
        var runner = fixture.CreateRunner();
        runner.WriteIoStoreCsv = false;

        var exception = await Assert.ThrowsExceptionAsync<PackageVerificationException>(
            () => new PackageVerifier(runner, TextWriter.Null).VerifyAsync(fixture.CreateRequest()));

        StringAssert.Contains(exception.Message, csvPath);
        Assert.IsFalse(File.Exists(csvPath));
    }
}
