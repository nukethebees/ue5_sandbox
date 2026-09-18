using System.Text;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using SetLiveCodingDisabled;

namespace SetLiveCodingDisabled.Tests;

[TestClass]
public sealed class LiveCodingSettingsNormalizerTests
{
    [TestMethod]
    public void Disable_changes_the_live_coding_value_and_preserves_unrelated_content()
    {
        using var directory = new TemporaryDirectory();
        var settings_path = Path.Combine(directory.Path, "EditorPerProjectUserSettings.ini");
        const string input = "[Other]\r\nbEnabled=True\r\n\r\n[/Script/LiveCoding.LiveCodingSettings]\r\nbEnabled = True ; keep\r\nbPreloadProjectModules=True\r\n\r\n[After]\r\nValue=Keep\r\n";
        const string expected = "[Other]\r\nbEnabled=True\r\n\r\n[/Script/LiveCoding.LiveCodingSettings]\r\nbEnabled = False ; keep\r\nbPreloadProjectModules=True\r\n\r\n[After]\r\nValue=Keep\r\n";
        File.WriteAllText(settings_path, input, new UTF8Encoding(false, true));

        var changed = new LiveCodingSettingsNormalizer().Disable(settings_path);

        Assert.IsTrue(changed);
        Assert.AreEqual(expected, File.ReadAllText(settings_path));
    }

    [TestMethod]
    public void Disable_preserves_trailing_comments_and_content()
    {
        using var directory = new TemporaryDirectory();
        var settings_path = Path.Combine(directory.Path, "Settings.ini");
        const string input = "[/Script/LiveCoding.LiveCodingSettings]\nbEnabled=True#keep\nOther=Value\n";
        const string expected = "[/Script/LiveCoding.LiveCodingSettings]\nbEnabled=False#keep\nOther=Value\n";
        File.WriteAllText(settings_path, input, new UTF8Encoding(false, true));

        new LiveCodingSettingsNormalizer().Disable(settings_path);

        Assert.AreEqual(expected, File.ReadAllText(settings_path));
    }

    [TestMethod]
    public void Disable_does_not_rewrite_an_already_disabled_file()
    {
        using var directory = new TemporaryDirectory();
        var settings_path = Path.Combine(directory.Path, "Settings.ini");
        const string input = "[/Script/LiveCoding.LiveCodingSettings]\nbEnabled=False\n[Other]\nbEnabled=True\n";
        File.WriteAllText(settings_path, input, new UTF8Encoding(false, true));
        var before_bytes = File.ReadAllBytes(settings_path);

        var changed = new LiveCodingSettingsNormalizer().Disable(settings_path);

        CollectionAssert.AreEqual(before_bytes, File.ReadAllBytes(settings_path));
        Assert.IsFalse(changed);
    }

    [TestMethod]
    public void Disable_preserves_utf16_little_endian_bom_and_non_ascii_text()
    {
        using var directory = new TemporaryDirectory();
        var settings_path = Path.Combine(directory.Path, "Settings.ini");
        var utf16 = new UnicodeEncoding(false, true, true);
        const string input = "[/Script/LiveCoding.LiveCodingSettings]\r\nbEnabled=True\r\nName=Ångström\r\n";
        const string expected = "[/Script/LiveCoding.LiveCodingSettings]\r\nbEnabled=False\r\nName=Ångström\r\n";
        File.WriteAllText(settings_path, input, utf16);

        new LiveCodingSettingsNormalizer().Disable(settings_path);

        var bytes = File.ReadAllBytes(settings_path);
        Assert.IsTrue(bytes.Length >= 2);
        Assert.AreEqual(0xff, bytes[0]);
        Assert.AreEqual(0xfe, bytes[1]);
        Assert.AreEqual(expected, File.ReadAllText(settings_path, utf16));
    }

    [TestMethod]
    public void Disable_preserves_utf8_bom()
    {
        using var directory = new TemporaryDirectory();
        var settings_path = Path.Combine(directory.Path, "Settings.ini");
        var utf8_with_bom = new UTF8Encoding(true, true);
        const string input = "[/Script/LiveCoding.LiveCodingSettings]\nbEnabled=True\n";
        const string expected = "[/Script/LiveCoding.LiveCodingSettings]\nbEnabled=False\n";
        File.WriteAllText(settings_path, input, utf8_with_bom);

        new LiveCodingSettingsNormalizer().Disable(settings_path);

        var bytes = File.ReadAllBytes(settings_path);
        Assert.IsTrue(bytes.Length >= 3);
        Assert.AreEqual(0xef, bytes[0]);
        Assert.AreEqual(0xbb, bytes[1]);
        Assert.AreEqual(0xbf, bytes[2]);
        Assert.AreEqual(expected, File.ReadAllText(settings_path, utf8_with_bom));
    }

    [TestMethod]
    public void Disable_does_not_create_a_missing_settings_file()
    {
        using var directory = new TemporaryDirectory();
        var settings_path = Path.Combine(directory.Path, "Missing.ini");

        var changed = new LiveCodingSettingsNormalizer().Disable(settings_path);

        Assert.IsFalse(changed);
        Assert.IsFalse(File.Exists(settings_path));
    }

    [TestMethod]
    public void Disable_preserves_mixed_line_endings()
    {
        using var directory = new TemporaryDirectory();
        var settings_path = Path.Combine(directory.Path, "Settings.ini");
        const string input = "[Other]\r\nbEnabled=True\n[/script/livecoding.livecodingsettings]\rbEnabled = tRuE ; keep\r\n[After]\nValue=Keep";
        const string expected = "[Other]\r\nbEnabled=True\n[/script/livecoding.livecodingsettings]\rbEnabled = False ; keep\r\n[After]\nValue=Keep";
        File.WriteAllText(settings_path, input, new UTF8Encoding(false, true));

        new LiveCodingSettingsNormalizer().Disable(settings_path);

        Assert.AreEqual(expected, File.ReadAllText(settings_path));
    }

    [TestMethod]
    public void Disable_changes_case_insensitive_target_section_and_key()
    {
        using var directory = new TemporaryDirectory();
        var settings_path = Path.Combine(directory.Path, "Settings.ini");
        const string input = "[/SCRIPT/LIVECODING.LIVECODINGSETTINGS]\nBENABLED=True\n";
        const string expected = "[/SCRIPT/LIVECODING.LIVECODINGSETTINGS]\nBENABLED=False\n";
        File.WriteAllText(settings_path, input, new UTF8Encoding(false, true));

        new LiveCodingSettingsNormalizer().Disable(settings_path);

        Assert.AreEqual(expected, File.ReadAllText(settings_path));
    }

    [TestMethod]
    public void Disable_does_not_change_similarly_named_keys()
    {
        using var directory = new TemporaryDirectory();
        var settings_path = Path.Combine(directory.Path, "Settings.ini");
        const string input = "[/Script/LiveCoding.LiveCodingSettings]\nbEnabledExtra=True\nnotbEnabled=True\nbEnabled=True\n";
        const string expected = "[/Script/LiveCoding.LiveCodingSettings]\nbEnabledExtra=True\nnotbEnabled=True\nbEnabled=False\n";
        File.WriteAllText(settings_path, input, new UTF8Encoding(false, true));

        new LiveCodingSettingsNormalizer().Disable(settings_path);

        Assert.AreEqual(expected, File.ReadAllText(settings_path));
    }

    [TestMethod]
    public void Disable_does_not_rewrite_a_target_section_without_bEnabled()
    {
        using var directory = new TemporaryDirectory();
        var settings_path = Path.Combine(directory.Path, "Settings.ini");
        const string input = "[/Script/LiveCoding.LiveCodingSettings]\nOther=Value\n";
        File.WriteAllText(settings_path, input, new UTF8Encoding(false, true));
        var before_bytes = File.ReadAllBytes(settings_path);

        var changed = new LiveCodingSettingsNormalizer().Disable(settings_path);

        Assert.IsFalse(changed);
        CollectionAssert.AreEqual(before_bytes, File.ReadAllBytes(settings_path));
    }

    [TestMethod]
    public void Disable_changes_bEnabled_in_each_target_section_only()
    {
        using var directory = new TemporaryDirectory();
        var settings_path = Path.Combine(directory.Path, "Settings.ini");
        const string input = "[First]\nbEnabled=True\n[/Script/LiveCoding.LiveCodingSettings]\nbEnabled=True\n[Second]\nbEnabled=True\n[/Script/LiveCoding.LiveCodingSettings]\nbEnabled=True\n";
        const string expected = "[First]\nbEnabled=True\n[/Script/LiveCoding.LiveCodingSettings]\nbEnabled=False\n[Second]\nbEnabled=True\n[/Script/LiveCoding.LiveCodingSettings]\nbEnabled=False\n";
        File.WriteAllText(settings_path, input, new UTF8Encoding(false, true));

        new LiveCodingSettingsNormalizer().Disable(settings_path);

        Assert.AreEqual(expected, File.ReadAllText(settings_path));
    }

    private sealed class TemporaryDirectory : IDisposable
    {
        public TemporaryDirectory()
        {
            Path = Directory.CreateTempSubdirectory("SandboxLiveCoding-").FullName;
        }

        public string Path { get; }

        public void Dispose()
        {
            Directory.Delete(Path, true);
        }
    }
}
