using CodeFormatTools;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace CodeFormatTools.Tests;

[TestClass]
public sealed class GitFileSelectorTests
{
    [TestMethod]
    public async Task SelectStagedAsync_excludes_unstaged_only_files()
    {
        using var fixture = new TemporaryGitRepository();
        var staged = fixture.WriteFile("Source/staged.cpp");
        fixture.WriteFile("Source/unstaged.cpp");
        fixture.RunGit("add", "Source/staged.cpp");

        var files = await CreateSelector().SelectStagedAsync(fixture.Root, fixture.CreateScope(), CancellationToken.None);

        CollectionAssert.AreEqual(new[] { staged }, files.ToArray());
    }

    [TestMethod]
    public async Task SelectChangedAsync_includes_worktree_changes_and_untracked_nonignored_files()
    {
        using var fixture = new TemporaryGitRepository();
        var changed = fixture.WriteFile("Source/changed.cpp");
        fixture.CommitAll();
        fixture.WriteFile("Source/changed.cpp", "int changed;\n");
        var untracked = fixture.WriteFile("Source/untracked.hlsl");
        fixture.WriteFile(".gitignore", "Source/ignored.cpp\n");
        fixture.WriteFile("Source/ignored.cpp");

        var files = await CreateSelector().SelectChangedAsync(fixture.Root, fixture.CreateScope(), CancellationToken.None);

        CollectionAssert.AreEquivalent(new[] { changed, untracked }, files.ToArray());
    }

    [TestMethod]
    public async Task SelectStagedAsync_uses_acmr_filter_and_ignores_unsupported_files()
    {
        using var fixture = new TemporaryGitRepository();
        fixture.WriteFile("Source/deleted.h");
        fixture.WriteFile("Source/notes.txt");
        fixture.CommitAll();
        File.Delete(fixture.PathFor("Source/deleted.h"));
        fixture.WriteFile("Source/notes.txt", "changed\n");
        fixture.RunGit("add", "--all");

        var files = await CreateSelector().SelectStagedAsync(fixture.Root, fixture.CreateScope(), CancellationToken.None);

        Assert.AreEqual(0, files.Count);
    }

    [TestMethod]
    public async Task GetRepositoryRootAsync_resolves_a_repository_with_spaces_from_a_subdirectory()
    {
        using var fixture = new TemporaryGitRepository("Format Root With Spaces");
        var nested = fixture.PathFor("Source/nested");
        Directory.CreateDirectory(nested);

        var root = await CreateSelector().GetRepositoryRootAsync(nested, CancellationToken.None);

        Assert.AreEqual(fixture.Root, root);
    }

    [TestMethod]
    public async Task SelectUnstagedAsync_detects_selected_staged_files_with_worktree_edits()
    {
        using var fixture = new TemporaryGitRepository();
        var mixed = fixture.WriteFile("Source/mixed.cpp");
        fixture.CommitAll();
        fixture.WriteFile("Source/mixed.cpp", "int staged;\n");
        fixture.RunGit("add", "Source/mixed.cpp");
        fixture.WriteFile("Source/mixed.cpp", "int unstaged;\n");

        var selector = CreateSelector();
        var selected = await selector.SelectStagedAsync(fixture.Root, fixture.CreateScope(), CancellationToken.None);
        var conflicts = await selector.SelectUnstagedAsync(fixture.Root, selected, CancellationToken.None);

        CollectionAssert.AreEqual(new[] { mixed }, conflicts.ToArray());
    }

    private static GitFileSelector CreateSelector()
    {
        return new GitFileSelector(new ProcessRunner());
    }
}
