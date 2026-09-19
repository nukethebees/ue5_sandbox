using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace AgentGit.Tests;

[TestClass]
public sealed class AgentGitOperationTests
{
    [TestMethod]
    public async Task Rebase_base_rebases_only_feature_and_does_not_update_sibling_refs()
    {
        using var fixture = new TemporaryAgentGitRepository();
        var feature = fixture.CreateWorktree("feature/rebase", "dev1");
        fixture.WriteFile("feature.txt", "feature\n", feature);
        fixture.RunGitAt(feature, "add", "feature.txt");
        fixture.RunGitAt(feature, "commit", "-qm", "feature");
        var original_feature = fixture.RunGit("rev-parse", "feature/rebase").Trim();
        fixture.RunGit("branch", "sibling", "feature/rebase");
        fixture.RunGit("config", "rebase.updateRefs", "true");
        fixture.WriteFile("base.txt", "base\n");
        fixture.RunGit("add", "base.txt");
        fixture.RunGit("commit", "-qm", "base");

        var result = await fixture.RunAgentGitAsync(feature, "rebase-base");

        Assert.AreEqual(ExitCodes.Success, result.ExitCode, result.Error);
        fixture.RunGit("merge-base", "--is-ancestor", "dev", "feature/rebase");
        Assert.AreEqual(original_feature, fixture.RunGit("rev-parse", "sibling").Trim());
        Assert.AreNotEqual(original_feature, fixture.RunGit("rev-parse", "feature/rebase").Trim());
    }

    [TestMethod]
    public async Task Rebase_base_stops_on_conflict_without_automatic_recovery()
    {
        using var fixture = new TemporaryAgentGitRepository();
        var feature = fixture.CreateWorktree("feature/conflict", "dev1");
        fixture.WriteFile("README.md", "feature\n", feature);
        fixture.RunGitAt(feature, "add", "README.md");
        fixture.RunGitAt(feature, "commit", "-qm", "feature change");
        fixture.WriteFile("README.md", "base\n");
        fixture.RunGit("add", "README.md");
        fixture.RunGit("commit", "-qm", "base change");

        var result = await fixture.RunAgentGitAsync(feature, "rebase-base");

        Assert.AreEqual(ExitCodes.GitFailure, result.ExitCode);
        StringAssert.Contains(result.Error, "could not apply");
        Assert.IsTrue(
            Directory.Exists(Path.Combine(fixture.RunGitAt(feature, "rev-parse", "--git-dir").Trim(), "rebase-merge")) ||
            Directory.Exists(Path.Combine(fixture.RunGitAt(feature, "rev-parse", "--git-dir").Trim(), "rebase-apply")));
    }

    [TestMethod]
    public async Task Branch_delete_removes_only_a_merged_unowned_feature_branch()
    {
        using var fixture = new TemporaryAgentGitRepository();
        fixture.RunGit("branch", "dev1", "dev");
        var worktree = fixture.CreateWorktree("feature/delete", "dev1");
        fixture.WriteFile("feature.txt", "feature\n", worktree);
        fixture.RunGitAt(worktree, "add", "feature.txt");
        fixture.RunGitAt(worktree, "commit", "-qm", "feature");
        fixture.RunGit("merge", "--no-ff", "-m", "merge feature", "feature/delete");
        var switch_result = await fixture.RunAgentGitAsync(worktree, "switch", "dev1");
        Assert.AreEqual(ExitCodes.Success, switch_result.ExitCode, switch_result.Error);

        var delete = await fixture.RunAgentGitAsync(worktree, "branch-delete", "feature/delete");

        Assert.AreEqual(ExitCodes.Success, delete.ExitCode, delete.Error);
        Assert.AreEqual(string.Empty, fixture.RunGit("branch", "--list", "feature/delete"));
        Assert.AreEqual("dev\n", fixture.RunGit("branch", "--show-current"));
        Assert.AreEqual("dev1\n", fixture.RunGitAt(worktree, "branch", "--show-current"));
    }

    [TestMethod]
    public async Task Branch_delete_rejects_unmerged_protected_workspace_current_and_owned_branches()
    {
        using var fixture = new TemporaryAgentGitRepository();
        fixture.RunGit("branch", "dev1", "dev");
        var worktree = fixture.CreateWorktree("feature/owned", "dev1");

        var owned = await fixture.RunAgentGitAsync(fixture.RepositoryRoot, "branch-delete", "feature/owned");
        var protected_result = await fixture.RunAgentGitAsync(fixture.RepositoryRoot, "branch-delete", "dev");
        var workspace_result = await fixture.RunAgentGitAsync(fixture.RepositoryRoot, "branch-delete", "dev1");

        Assert.AreEqual(ExitCodes.PolicyDenied, owned.ExitCode);
        StringAssert.Contains(owned.Output, "checked out in worktree");
        Assert.AreEqual(ExitCodes.PolicyDenied, protected_result.ExitCode);
        Assert.AreEqual(ExitCodes.PolicyDenied, workspace_result.ExitCode);

        fixture.WriteFile("unmerged.txt", "change\n", worktree);
        fixture.RunGitAt(worktree, "add", "unmerged.txt");
        fixture.RunGitAt(worktree, "commit", "-qm", "unmerged");
        var current = await fixture.RunAgentGitAsync(worktree, "branch-delete", "feature/owned");
        Assert.AreEqual(ExitCodes.PolicyDenied, current.ExitCode);
        StringAssert.Contains(current.Output, "Current branch");

        var switch_result = await fixture.RunAgentGitAsync(worktree, "switch", "dev1");
        Assert.AreEqual(ExitCodes.Success, switch_result.ExitCode, switch_result.Error);
        var unmerged = await fixture.RunAgentGitAsync(worktree, "branch-delete", "feature/owned");
        Assert.AreEqual(ExitCodes.PolicyDenied, unmerged.ExitCode);
        StringAssert.Contains(unmerged.Output, "not fully merged");
    }

    [TestMethod]
    public async Task Switch_rejects_target_checked_out_in_another_worktree()
    {
        using var fixture = new TemporaryAgentGitRepository();
        var other = fixture.CreateWorktree("feature/owned", "dev1");

        var result = await fixture.RunAgentGitAsync(fixture.RepositoryRoot, "switch", "feature/owned");

        Assert.AreEqual(ExitCodes.PolicyDenied, result.ExitCode);
        StringAssert.Contains(result.Output.Replace('/', Path.DirectorySeparatorChar), other);
        Assert.AreEqual("dev\n", fixture.RunGit("branch", "--show-current"));
    }

    [TestMethod]
    public async Task Mutations_are_denied_in_detached_and_in_progress_states()
    {
        using var fixture = new TemporaryAgentGitRepository();
        fixture.RunGit("switch", "--detach", "HEAD");

        var detached = await fixture.RunAgentGitAsync(fixture.RepositoryRoot, "commit", "-m", "detached");

        Assert.AreEqual(ExitCodes.PolicyDenied, detached.ExitCode);
        StringAssert.Contains(detached.Output, "detached HEAD");

        fixture.RunGit("switch", "-qc", "feature/in-progress");
        var git_directory = Path.GetFullPath(fixture.RunGit("rev-parse", "--git-dir").Trim(), fixture.RepositoryRoot);
        File.WriteAllText(Path.Combine(git_directory, "MERGE_HEAD"),
            fixture.RunGit("rev-parse", "HEAD").Trim());
        var in_progress = await fixture.RunAgentGitAsync(fixture.RepositoryRoot, "add-all");
        Assert.AreEqual(ExitCodes.PolicyDenied, in_progress.ExitCode);
        StringAssert.Contains(in_progress.Output, "already in progress");
    }

    [TestMethod]
    public async Task Add_rejects_paths_outside_worktree_and_treats_pathspec_magic_literally()
    {
        using var fixture = new TemporaryAgentGitRepository();
        fixture.RunGit("switch", "-qc", "feature/paths");
        var outside = fixture.WriteFile("outside.txt", "outside\n", Path.GetDirectoryName(fixture.RepositoryRoot)!);

        var outside_result = await fixture.RunAgentGitAsync(fixture.RepositoryRoot, "add", outside);

        Assert.AreEqual(ExitCodes.PolicyDenied, outside_result.ExitCode);
        StringAssert.Contains(outside_result.Output, "outside the current worktree");

        const string magic_name = "[a].txt";
        fixture.WriteFile(magic_name, "literal\n");
        fixture.WriteFile("a.txt", "must remain untracked\n");
        var literal = await fixture.RunAgentGitAsync(fixture.RepositoryRoot, "add", magic_name);
        Assert.AreEqual(ExitCodes.Success, literal.ExitCode, literal.Error);
        StringAssert.Contains(fixture.RunGit("diff", "--cached", "--name-only"), magic_name);
        StringAssert.Contains(fixture.RunGit("status", "--porcelain"), "?? a.txt");
    }
}
