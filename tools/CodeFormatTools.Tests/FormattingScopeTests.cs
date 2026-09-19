using System.Text;
using CodeFormatTools;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace CodeFormatTools.Tests;

[TestClass]
public sealed class FormattingScopeTests
{
    [TestMethod]
    public void SelectAll_recursively_finds_supported_files_and_shaders()
    {
        using var fixture = new TemporaryGitRepository();
        var source = fixture.WriteFile("Source/all.cpp");
        var shader = fixture.WriteFile("Source/shaders/effect.usf");
        fixture.WriteFile("Source/shaders/include.ush");
        fixture.WriteFile("Source/notes.txt");

        var scope = fixture.CreateScope();
        var files = scope.SelectAll(_ => { });

        CollectionAssert.AreEquivalent(
            new[] { source, shader, fixture.PathFor("Source/shaders/include.ush") },
            files.ToArray());
    }

    [TestMethod]
    public void IsFormatCandidate_excludes_case_insensitive_path_components_only()
    {
        using var fixture = new TemporaryGitRepository();
        fixture.WriteFile("Source/Generated/ignored.cpp");
        fixture.WriteFile("Source/ThirdParty/ignored.h");
        fixture.WriteFile("Source/Third_Party/ignored.hpp");
        var retained = fixture.WriteFile("Source/NotGenerated/retained.cpp");

        var scope = fixture.CreateScope();

        Assert.IsFalse(scope.IsFormatCandidate(fixture.PathFor("Source/Generated/ignored.cpp")));
        Assert.IsFalse(scope.IsFormatCandidate(fixture.PathFor("Source/ThirdParty/ignored.h")));
        Assert.IsFalse(scope.IsFormatCandidate(fixture.PathFor("Source/Third_Party/ignored.hpp")));
        Assert.IsTrue(scope.IsFormatCandidate(retained));
    }

    [TestMethod]
    public void IsFormatCandidate_requires_a_real_directory_boundary()
    {
        using var fixture = new TemporaryGitRepository();
        var outside = fixture.WriteFile("PluginsElse/outside.cpp");
        var inside = fixture.WriteFile("Plugins/inside.cpp");
        var scope = new FormattingScope(fixture.Root, ["Plugins"]);

        Assert.IsFalse(scope.IsFormatCandidate(outside));
        Assert.IsTrue(scope.IsFormatCandidate(inside));
    }

    [TestMethod]
    public void ParseNullDelimitedPaths_preserves_spaces()
    {
        using var fixture = new TemporaryGitRepository("Repository With Spaces");
        var output = Encoding.UTF8.GetBytes("Source/file with spaces.cpp\0Source/next.h\0");

        var paths = GitFileSelector.ParseNullDelimitedPaths(output, fixture.Root);

        CollectionAssert.AreEqual(
            new[] { fixture.PathFor("Source/file with spaces.cpp"), fixture.PathFor("Source/next.h") },
            paths.ToArray());
    }

    [TestMethod]
    public void Normalize_converts_crlf_and_lone_cr_without_rewriting_lf()
    {
        using var fixture = new TemporaryGitRepository();
        var normalized = fixture.PathFor("Source/newlines.cpp");
        File.WriteAllBytes(normalized, "one\r\ntwo\rthree\n"u8.ToArray());

        Assert.IsTrue(NewlineNormalizer.Normalize(normalized));
        CollectionAssert.AreEqual("one\ntwo\nthree\n"u8.ToArray(), File.ReadAllBytes(normalized));
        var before = File.ReadAllBytes(normalized);
        Assert.IsFalse(NewlineNormalizer.Normalize(normalized));
        CollectionAssert.AreEqual(before, File.ReadAllBytes(normalized));
    }

    [TestMethod]
    public void FormatRequest_rejects_conflicting_and_unknown_arguments()
    {
        Assert.IsFalse(FormatRequest.TryParse(["--staged", "--changed"], out _, out var conflict));
        StringAssert.Contains(conflict, "Only one");
        Assert.IsFalse(FormatRequest.TryParse(["--format-hlsl"], out _, out var unknown));
        StringAssert.Contains(unknown, "Unknown");
    }
}
