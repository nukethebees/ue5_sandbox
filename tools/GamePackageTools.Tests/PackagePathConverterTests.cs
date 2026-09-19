using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace GamePackageTools.Tests;

[TestClass]
public sealed class PackagePathConverterTests
{
    [TestMethod]
    public void ToUnrealPackagePath_converts_project_content_assets()
    {
        using var fixture = new PackageToolFixture();
        var asset = fixture.WriteProjectFile("Content/UI/DA_ui_data.uasset");

        var packagePath = new PackagePathConverter(fixture.ProjectRoot).ToUnrealPackagePath(asset);

        Assert.AreEqual("/Game/UI/DA_ui_data", packagePath);
    }

    [TestMethod]
    public void ToUnrealPackagePath_converts_plugin_content_assets_from_the_descriptor_name()
    {
        using var fixture = new PackageToolFixture();
        fixture.WriteProjectFile("Plugins/TestPlugin/TestPlugin.uplugin", "{}");
        var asset = fixture.WriteProjectFile("Plugins/TestPlugin/Content/UI/Widget.uasset");

        var packagePath = new PackagePathConverter(fixture.ProjectRoot).ToUnrealPackagePath(asset);

        Assert.AreEqual("/TestPlugin/UI/Widget", packagePath);
    }

    [TestMethod]
    public void ToUnrealPackagePath_rejects_assets_outside_content()
    {
        using var fixture = new PackageToolFixture();
        var asset = fixture.WriteProjectFile("Source/Outside.uasset");

        var exception = Assert.ThrowsException<PackageVerificationException>(
            () => new PackagePathConverter(fixture.ProjectRoot).ToUnrealPackagePath(asset));

        StringAssert.Contains(exception.Message, "not below a Content directory");
    }

    [TestMethod]
    public void ToUnrealPackagePath_rejects_plugin_assets_without_a_descriptor()
    {
        using var fixture = new PackageToolFixture();
        var asset = fixture.WriteProjectFile("Plugins/NoDescriptor/Content/UI/Widget.uasset");

        var exception = Assert.ThrowsException<PackageVerificationException>(
            () => new PackagePathConverter(fixture.ProjectRoot).ToUnrealPackagePath(asset));

        StringAssert.Contains(exception.Message, "Could not find the plugin descriptor");
    }

    [TestMethod]
    public void ToStagedAssetPath_converts_game_and_plugin_paths()
    {
        Assert.AreEqual("../../../Sandbox/Content/Foo/Bar", PackagePathConverter.ToStagedAssetPath("/Game/Foo/Bar"));
        Assert.AreEqual("../../../Sandbox/Plugins/SpaceGame/Content/Foo/Bar", PackagePathConverter.ToStagedAssetPath("/SpaceGame/Foo/Bar"));
    }

    [DataTestMethod]
    [DataRow("Game/Foo")]
    [DataRow("/Game")]
    [DataRow("/Game/")]
    [DataRow("//Foo")]
    public void ToStagedAssetPath_rejects_invalid_paths(string packagePath)
    {
        Assert.ThrowsException<PackageVerificationException>(() => PackagePathConverter.ToStagedAssetPath(packagePath));
    }
}
