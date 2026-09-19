using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace AgentGit.Tests;

[TestClass]
public sealed class PatchIdentityTests
{
    [TestMethod]
    public async Task Identical_patch_on_different_parent_has_same_fingerprint()
    {
        using var fixture = new TemporaryAgentGitRepository();
        var original_base = fixture.RunGit("rev-parse", "dev").Trim();
        fixture.RunGit("switch", "-qc", "feature/first");
        fixture.WriteFile("feature.txt", "same patch\n");
        fixture.RunGit("add", "feature.txt");
        fixture.RunGit("commit", "-qm", "feature");
        var first_tip = fixture.RunGit("rev-parse", "HEAD").Trim();

        fixture.RunGit("switch", "dev");
        fixture.WriteFile("unrelated.txt", "base moved\n");
        fixture.RunGit("add", "unrelated.txt");
        fixture.RunGit("commit", "-qm", "move base");
        var moved_base = fixture.RunGit("rev-parse", "HEAD").Trim();
        fixture.RunGit("switch", "-qc", "feature/second");
        fixture.WriteFile("feature.txt", "same patch\n");
        fixture.RunGit("add", "feature.txt");
        fixture.RunGit("commit", "-qm", "same feature");
        var second_tip = fixture.RunGit("rev-parse", "HEAD").Trim();

        var service = CreateService(fixture);
        var first = await service.ComputeAsync(fixture.RepositoryRoot, original_base, first_tip, default);
        var second = await service.ComputeAsync(fixture.RepositoryRoot, moved_base, second_tip, default);

        Assert.AreEqual(first.Fingerprint, second.Fingerprint);
    }

    [TestMethod]
    public async Task Squashed_history_with_same_effective_patch_has_same_fingerprint()
    {
        using var fixture = new TemporaryAgentGitRepository();
        var base_commit = fixture.RunGit("rev-parse", "dev").Trim();
        fixture.RunGit("switch", "-qc", "feature/multiple");
        fixture.WriteFile("one.txt", "one\n");
        fixture.RunGit("add", "one.txt");
        fixture.RunGit("commit", "-qm", "one");
        fixture.WriteFile("two.txt", "two\n");
        fixture.RunGit("add", "two.txt");
        fixture.RunGit("commit", "-qm", "two");
        var multiple_tip = fixture.RunGit("rev-parse", "HEAD").Trim();

        fixture.RunGit("switch", "dev");
        fixture.RunGit("switch", "-qc", "feature/squashed");
        fixture.WriteFile("two.txt", "two\n");
        fixture.WriteFile("one.txt", "one\n");
        fixture.RunGit("add", "one.txt", "two.txt");
        fixture.RunGit("commit", "-qm", "squashed");
        var squashed_tip = fixture.RunGit("rev-parse", "HEAD").Trim();

        var service = CreateService(fixture);
        var multiple = await service.ComputeAsync(fixture.RepositoryRoot, base_commit, multiple_tip, default);
        var squashed = await service.ComputeAsync(fixture.RepositoryRoot, base_commit, squashed_tip, default);

        Assert.AreEqual(multiple.Fingerprint, squashed.Fingerprint);
    }

    [TestMethod]
    public async Task Identical_edit_in_different_function_has_different_fingerprint()
    {
        using var fixture = new TemporaryAgentGitRepository();
        fixture.WriteFile(
            "duplicate.cpp",
            """
            void First()
            {
                enabled = false;
            }

            void Second()
            {
                enabled = false;
            }

            """);
        fixture.RunGit("add", "duplicate.cpp");
        fixture.RunGit("commit", "-qm", "add duplicate regions");
        var base_commit = fixture.RunGit("rev-parse", "HEAD").Trim();

        fixture.RunGit("switch", "-qc", "feature/first-occurrence");
        fixture.WriteFile(
            "duplicate.cpp",
            """
            void First()
            {
                enabled = true;
            }

            void Second()
            {
                enabled = false;
            }

            """);
        fixture.RunGit("add", "duplicate.cpp");
        fixture.RunGit("commit", "-qm", "change first occurrence");
        var first_tip = fixture.RunGit("rev-parse", "HEAD").Trim();

        fixture.RunGit("switch", "dev");
        fixture.RunGit("switch", "-qc", "feature/second-occurrence");
        fixture.WriteFile(
            "duplicate.cpp",
            """
            void First()
            {
                enabled = false;
            }

            void Second()
            {
                enabled = true;
            }

            """);
        fixture.RunGit("add", "duplicate.cpp");
        fixture.RunGit("commit", "-qm", "change second occurrence");
        var second_tip = fixture.RunGit("rev-parse", "HEAD").Trim();

        var service = CreateService(fixture);
        var first = await service.ComputeAsync(fixture.RepositoryRoot, base_commit, first_tip, default);
        var second = await service.ComputeAsync(fixture.RepositoryRoot, base_commit, second_tip, default);

        Assert.AreNotEqual(first.Fingerprint, second.Fingerprint);
    }

    [TestMethod]
    public async Task Meaningful_or_whitespace_change_invalidates_fingerprint()
    {
        using var fixture = new TemporaryAgentGitRepository();
        var base_commit = fixture.RunGit("rev-parse", "dev").Trim();
        var service = CreateService(fixture);
        var fingerprints = new List<string>();
        foreach (var (branch, contents) in new[]
                 {
                     ("feature/first", "value\n"),
                     ("feature/line", "changed\n"),
                     ("feature/space", "value \n"),
                 })
        {
            fixture.RunGit("switch", "dev");
            fixture.RunGit("switch", "-qc", branch);
            fixture.WriteFile("value.txt", contents);
            fixture.RunGit("add", "value.txt");
            fixture.RunGit("commit", "-qm", branch);
            fingerprints.Add((await service.ComputeAsync(
                fixture.RepositoryRoot, base_commit, "HEAD", default)).Fingerprint);
        }

        Assert.AreEqual(3, fingerprints.Distinct(StringComparer.Ordinal).Count());
    }

    [TestMethod]
    public async Task File_addition_and_deletion_are_part_of_fingerprint_but_commit_metadata_is_not()
    {
        using var fixture = new TemporaryAgentGitRepository();
        var base_commit = fixture.RunGit("rev-parse", "dev").Trim();
        fixture.RunGit("switch", "-qc", "feature/files");
        fixture.WriteFile("added.txt", "added\n");
        fixture.RunGit("rm", "README.md");
        fixture.RunGit("add", "added.txt");
        fixture.RunGit("commit", "-qm", "files");
        var first_tip = fixture.RunGit("rev-parse", "HEAD").Trim();
        fixture.RunGit("commit", "--amend", "-qm", "different metadata");
        var amended_tip = fixture.RunGit("rev-parse", "HEAD").Trim();

        var service = CreateService(fixture);
        var first = await service.ComputeAsync(fixture.RepositoryRoot, base_commit, first_tip, default);
        var amended = await service.ComputeAsync(fixture.RepositoryRoot, base_commit, amended_tip, default);

        Assert.AreEqual(first.Fingerprint, amended.Fingerprint);
        CollectionAssert.AreEquivalent(new[] { "README.md", "added.txt" }, first.ChangedPaths.ToArray());
    }

    [TestMethod]
    public async Task Conflict_resolution_that_changes_effective_patch_invalidates_review_identity()
    {
        using var fixture = new TemporaryAgentGitRepository();
        var original_base = fixture.RunGit("rev-parse", "dev").Trim();
        fixture.RunGit("switch", "-qc", "feature/conflict");
        fixture.WriteFile("README.md", "feature version\n");
        fixture.RunGit("add", "README.md");
        fixture.RunGit("commit", "-qm", "feature version");
        var original_tip = fixture.RunGit("rev-parse", "HEAD").Trim();

        fixture.RunGit("switch", "dev");
        fixture.WriteFile("README.md", "new base version\n");
        fixture.RunGit("add", "README.md");
        fixture.RunGit("commit", "-qm", "new base version");
        var new_base = fixture.RunGit("rev-parse", "HEAD").Trim();
        fixture.RunGit("switch", "feature/conflict");
        var rebase = fixture.RunGitAllowFailureAt(fixture.RepositoryRoot, "rebase", "dev");
        Assert.AreNotEqual(0, rebase.ExitCode);
        fixture.WriteFile("README.md", "resolved combined version\n");
        fixture.RunGit("add", "README.md");
        fixture.RunGit("-c", "core.editor=true", "rebase", "--continue");

        var service = CreateService(fixture);
        var reviewed = await service.ComputeAsync(
            fixture.RepositoryRoot, original_base, original_tip, default);
        var resolved = await service.ComputeAsync(
            fixture.RepositoryRoot, new_base, "HEAD", default);

        Assert.AreNotEqual(reviewed.Fingerprint, resolved.Fingerprint);
    }

    private static PatchIdentityService CreateService(TemporaryAgentGitRepository fixture) =>
        new(new GitClient(fixture.Trust, new ProcessRunner()));
}
