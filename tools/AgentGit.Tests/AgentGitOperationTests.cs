using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace AgentGit.Tests;

[TestClass]
public sealed class AgentGitOperationTests
{
    [TestMethod]
    public async Task Executor_rejects_ref_changes_after_policy_evaluation()
    {
        using var fixture = new TemporaryAgentGitRepository();
        fixture.RunGit("switch", "-qc", "feature/evaluated");
        fixture.WriteFile("change.txt", "change\n");
        fixture.RunGit("add", "change.txt");
        var git = new GitClient(fixture.Trust, new ProcessRunner());
        var discovery = new RepositoryDiscovery(git);
        var context = await discovery.DiscoverAsync(fixture.Trust, fixture.RepositoryRoot);
        var evaluated = await new PolicyEvaluator(discovery).EvaluateAsync(
            new CommitRequest(false, "must not commit"),
            context,
            fixture.RepositoryRoot);
        Assert.IsTrue(evaluated.Decision.Allowed);
        fixture.RunGit("switch", "-qc", "feature/raced");

        var exception = await Assert.ThrowsExceptionAsync<RepositoryStateException>(
            async () => await new OperationExecutor(git, discovery).ExecuteAsync(evaluated));

        StringAssert.Contains(exception.Message, "changed after policy evaluation");
        Assert.AreEqual("feature/raced\n", fixture.RunGit("branch", "--show-current"));
        Assert.AreEqual("initial\n", fixture.RunGit("log", "-1", "--format=%s"));
    }

    [TestMethod]
    public async Task Executor_rejects_symbolic_current_ref_introduced_after_policy_evaluation()
    {
        using var fixture = new TemporaryAgentGitRepository();
        fixture.RunGit("switch", "-qc", "feature/symbolic-race");
        fixture.WriteFile("change.txt", "change\n");
        fixture.RunGit("add", "change.txt");
        var protected_commit = fixture.RunGit("rev-parse", "dev").Trim();
        var git = new GitClient(fixture.Trust, new ProcessRunner());
        var discovery = new RepositoryDiscovery(git);
        var context = await discovery.DiscoverAsync(fixture.Trust, fixture.RepositoryRoot);
        var evaluated = await new PolicyEvaluator(discovery).EvaluateAsync(
            new CommitRequest(false, "must not commit"),
            context,
            fixture.RepositoryRoot);
        Assert.IsTrue(evaluated.Decision.Allowed);
        fixture.RunGit("symbolic-ref", "refs/heads/feature/symbolic-race", "refs/heads/dev");

        var exception = await Assert.ThrowsExceptionAsync<RepositoryStateException>(
            async () => await new OperationExecutor(git, discovery).ExecuteAsync(evaluated));

        StringAssert.Contains(exception.Message, "is symbolic");
        Assert.AreEqual(protected_commit, fixture.RunGit("rev-parse", "dev").Trim());
        Assert.AreEqual("initial\n", fixture.RunGit("log", "-1", "--format=%s"));
    }

    [TestMethod]
    public async Task Executor_rejects_target_branch_changes_after_policy_evaluation()
    {
        using var fixture = new TemporaryAgentGitRepository();
        fixture.RunGit("branch", "feature/delete-race", "dev");
        fixture.RunGit("switch", "-qc", "feature/unmerged");
        fixture.WriteFile("unmerged.txt", "unmerged\n");
        fixture.RunGit("add", "unmerged.txt");
        fixture.RunGit("commit", "-qm", "unmerged target");
        fixture.RunGit("switch", "-q", "dev");
        var git = new GitClient(fixture.Trust, new ProcessRunner());
        var discovery = new RepositoryDiscovery(git);
        var context = await discovery.DiscoverAsync(fixture.Trust, fixture.RepositoryRoot);
        var evaluated = await new PolicyEvaluator(discovery).EvaluateAsync(
            new BranchDeleteRequest(false, "feature/delete-race"),
            context,
            fixture.RepositoryRoot);
        Assert.IsTrue(evaluated.Decision.Allowed);
        fixture.RunGit("branch", "-f", "feature/delete-race", "feature/unmerged");

        var exception = await Assert.ThrowsExceptionAsync<RepositoryStateException>(
            async () => await new OperationExecutor(git, discovery).ExecuteAsync(evaluated));

        StringAssert.Contains(exception.Message, "Target branch 'feature/delete-race' changed");
        StringAssert.Contains(fixture.RunGit("branch", "--list", "feature/delete-race"), "feature/delete-race");
    }

    [TestMethod]
    public async Task Executor_rejects_replaced_base_worktree_before_branch_deletion()
    {
        using var fixture = new TemporaryAgentGitRepository();
        fixture.RunGit("switch", "-qc", "feature/current");
        var base_worktree = Path.Combine(fixture.InstallRoot, "base-worktree");
        fixture.RunGit("worktree", "add", "-q", base_worktree, "dev");
        fixture.RunGit("branch", "feature/delete-race", "dev");
        var git = new GitClient(fixture.Trust, new ProcessRunner());
        var discovery = new RepositoryDiscovery(git);
        var context = await discovery.DiscoverAsync(fixture.Trust, fixture.RepositoryRoot);
        var evaluated = await new PolicyEvaluator(discovery).EvaluateAsync(
            new BranchDeleteRequest(false, "feature/delete-race"),
            context,
            fixture.RepositoryRoot);
        Assert.IsTrue(evaluated.Decision.Allowed);

        fixture.RunGit("worktree", "remove", "--force", base_worktree);
        Directory.CreateDirectory(base_worktree);
        fixture.RunGitAt(base_worktree, "init", "-q", "--initial-branch=unrelated");
        fixture.RunGitAt(base_worktree, "config", "user.name", "Unrelated Test");
        fixture.RunGitAt(base_worktree, "config", "user.email", "unrelated@example.com");
        File.WriteAllText(Path.Combine(base_worktree, "unrelated.txt"), "unrelated\n");
        fixture.RunGitAt(base_worktree, "add", "unrelated.txt");
        fixture.RunGitAt(base_worktree, "commit", "-qm", "unrelated");
        fixture.RunGitAt(base_worktree, "branch", "feature/delete-race");

        var exception = await Assert.ThrowsExceptionAsync<RepositoryStateException>(
            async () => await new OperationExecutor(git, discovery).ExecuteAsync(evaluated));

        StringAssert.Contains(exception.Message, "no longer belongs to the evaluated repository");
        StringAssert.Contains(
            fixture.RunGitAt(base_worktree, "branch", "--list", "feature/delete-race"),
            "feature/delete-race");
        StringAssert.Contains(fixture.RunGit("branch", "--list", "feature/delete-race"), "feature/delete-race");
    }

    [TestMethod]
    public async Task Executor_rejects_index_changes_after_policy_evaluation()
    {
        using var fixture = new TemporaryAgentGitRepository();
        fixture.RunGit("switch", "-qc", "feature/index-race");
        fixture.WriteFile("change.txt", "evaluated\n");
        fixture.RunGit("add", "change.txt");
        var git = new GitClient(fixture.Trust, new ProcessRunner());
        var discovery = new RepositoryDiscovery(git);
        var context = await discovery.DiscoverAsync(fixture.Trust, fixture.RepositoryRoot);
        var evaluated = await new PolicyEvaluator(discovery).EvaluateAsync(
            new CommitRequest(false, "must not commit"),
            context,
            fixture.RepositoryRoot);
        Assert.IsTrue(evaluated.Decision.Allowed);
        fixture.WriteFile("change.txt", "raced\n");
        fixture.RunGit("add", "change.txt");

        var exception = await Assert.ThrowsExceptionAsync<RepositoryStateException>(
            async () => await new OperationExecutor(git, discovery).ExecuteAsync(evaluated));

        StringAssert.Contains(exception.Message, "index or working tree changed");
        Assert.AreEqual("initial\n", fixture.RunGit("log", "-1", "--format=%s"));
    }

    [TestMethod]
    public async Task Executor_rejects_executable_config_changes_after_policy_evaluation()
    {
        using var fixture = new TemporaryAgentGitRepository();
        fixture.RunGit("switch", "-qc", "feature/config-race");
        fixture.WriteFile("change.txt", "change\n");
        fixture.RunGit("add", "change.txt");
        var git = new GitClient(fixture.Trust, new ProcessRunner());
        var discovery = new RepositoryDiscovery(git);
        var context = await discovery.DiscoverAsync(fixture.Trust, fixture.RepositoryRoot);
        var evaluated = await new PolicyEvaluator(discovery).EvaluateAsync(
            new CommitRequest(false, "must not commit"),
            context,
            fixture.RepositoryRoot);
        Assert.IsTrue(evaluated.Decision.Allowed);
        fixture.RunGit("config", "filter.evil.clean", "arbitrary-command");

        var exception = await Assert.ThrowsExceptionAsync<RepositoryStateException>(
            async () => await new OperationExecutor(git, discovery).ExecuteAsync(evaluated));

        StringAssert.Contains(exception.Message, "unsupported executable settings");
        Assert.AreEqual("initial\n", fixture.RunGit("log", "-1", "--format=%s"));
    }

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
    public async Task Branch_delete_remains_available_from_a_protected_branch()
    {
        using var fixture = new TemporaryAgentGitRepository();
        fixture.RunGit("branch", "feature/already-merged", "dev");

        var result = await fixture.RunAgentGitAsync(
            fixture.RepositoryRoot,
            "branch-delete",
            "feature/already-merged");

        Assert.AreEqual(ExitCodes.Success, result.ExitCode, result.Error);
        Assert.AreEqual("dev\n", fixture.RunGit("branch", "--show-current"));
        Assert.AreEqual(string.Empty, fixture.RunGit("branch", "--list", "feature/already-merged"));
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
        fixture.RunGit("switch", "-qc", "feature/current");

        var result = await fixture.RunAgentGitAsync(fixture.RepositoryRoot, "switch", "feature/owned");

        Assert.AreEqual(ExitCodes.PolicyDenied, result.ExitCode);
        StringAssert.Contains(result.Output.Replace('/', Path.DirectorySeparatorChar), other);
        Assert.AreEqual("feature/current\n", fixture.RunGit("branch", "--show-current"));
    }

    [TestMethod]
    public async Task Switch_and_switch_create_reject_missing_existing_and_invalid_targets()
    {
        using var fixture = new TemporaryAgentGitRepository();
        fixture.RunGit("switch", "-qc", "feature/current");

        var missing = await fixture.RunAgentGitAsync(fixture.RepositoryRoot, "switch", "feature/missing");
        Assert.AreEqual(ExitCodes.PolicyDenied, missing.ExitCode, missing.Error);
        StringAssert.Contains(missing.Output, "does not exist");

        var create = await fixture.RunAgentGitAsync(
            fixture.RepositoryRoot,
            "switch-create",
            "feature/literal&branch");
        Assert.AreEqual(ExitCodes.Success, create.ExitCode, create.Error);
        Assert.AreEqual("feature/literal&branch\n", fixture.RunGit("branch", "--show-current"));

        var existing = await fixture.RunAgentGitAsync(
            fixture.RepositoryRoot,
            "switch-create",
            "feature/literal&branch");
        Assert.AreEqual(ExitCodes.PolicyDenied, existing.ExitCode, existing.Error);
        StringAssert.Contains(existing.Output, "already exists");

        var invalid = await fixture.RunAgentGitAsync(
            fixture.RepositoryRoot,
            "switch-create",
            "--upload-pack=arbitrary-command");
        Assert.AreEqual(ExitCodes.StateFailure, invalid.ExitCode, invalid.Error);
        StringAssert.Contains(invalid.Error, "Invalid local branch name");

        var pseudo_ref = await fixture.RunAgentGitAsync(
            fixture.RepositoryRoot,
            "switch-create",
            "HEAD");
        Assert.AreEqual(ExitCodes.StateFailure, pseudo_ref.ExitCode, pseudo_ref.Error);
        StringAssert.Contains(pseudo_ref.Error, "Invalid local branch name");
    }

    [TestMethod]
    public async Task Switch_create_is_allowed_from_workspace_but_rebase_is_feature_only()
    {
        using var fixture = new TemporaryAgentGitRepository();
        var workspace = fixture.CreateWorktree("dev1");

        var workspace_rebase = await fixture.RunAgentGitAsync(workspace, "rebase-base");
        Assert.AreEqual(ExitCodes.PolicyDenied, workspace_rebase.ExitCode, workspace_rebase.Error);
        StringAssert.Contains(workspace_rebase.Output, "workspace branch 'dev1'");

        var create = await fixture.RunAgentGitAsync(workspace, "switch-create", "feature/from-workspace");
        Assert.AreEqual(ExitCodes.Success, create.ExitCode, create.Error);
        Assert.AreEqual("feature/from-workspace\n", fixture.RunGitAt(workspace, "branch", "--show-current"));

        var protected_rebase = await fixture.RunAgentGitAsync(fixture.RepositoryRoot, "rebase-base");
        Assert.AreEqual(ExitCodes.PolicyDenied, protected_rebase.ExitCode, protected_rebase.Error);
        StringAssert.Contains(protected_rebase.Output, "protected branch 'dev'");
    }

    [TestMethod]
    public async Task Switch_supports_workspace_to_feature_feature_to_feature_and_return_to_own_workspace()
    {
        using var fixture = new TemporaryAgentGitRepository();
        var workspace = fixture.CreateWorktree("dev1");
        fixture.RunGit("branch", "feature/first", "dev");
        fixture.RunGit("branch", "feature/second", "dev");

        var from_workspace = await fixture.RunAgentGitAsync(workspace, "switch", "feature/first");
        Assert.AreEqual(ExitCodes.Success, from_workspace.ExitCode, from_workspace.Error);
        Assert.AreEqual("feature/first\n", fixture.RunGitAt(workspace, "branch", "--show-current"));

        var between_features = await fixture.RunAgentGitAsync(workspace, "switch", "feature/second");
        Assert.AreEqual(ExitCodes.Success, between_features.ExitCode, between_features.Error);
        Assert.AreEqual("feature/second\n", fixture.RunGitAt(workspace, "branch", "--show-current"));

        var return_home = await fixture.RunAgentGitAsync(workspace, "switch", "dev1");
        Assert.AreEqual(ExitCodes.Success, return_home.ExitCode, return_home.Error);
        Assert.AreEqual("dev1\n", fixture.RunGitAt(workspace, "branch", "--show-current"));
    }

    [TestMethod]
    public async Task Switch_and_rebase_preserve_ignored_untracked_files_that_targets_would_overwrite()
    {
        using var switch_fixture = new TemporaryAgentGitRepository();
        switch_fixture.RunGit("switch", "-qc", "feature/target");
        switch_fixture.WriteFile("ignored.txt", "target\n");
        switch_fixture.RunGit("add", "ignored.txt");
        switch_fixture.RunGit("commit", "-qm", "target file");
        switch_fixture.RunGit("switch", "-qc", "feature/current", "dev");
        switch_fixture.WriteFile(".gitignore", "ignored.txt\n");
        switch_fixture.RunGit("add", ".gitignore");
        switch_fixture.RunGit("commit", "-qm", "ignore local file");
        switch_fixture.WriteFile("ignored.txt", "local data\n");

        var switch_result = await switch_fixture.RunAgentGitAsync(
            switch_fixture.RepositoryRoot,
            "switch",
            "feature/target");

        Assert.AreEqual(ExitCodes.PolicyDenied, switch_result.ExitCode, switch_result.Error);
        StringAssert.Contains(switch_result.Output, "ignored.txt");
        Assert.AreEqual(
            "local data\n",
            File.ReadAllText(Path.Combine(switch_fixture.RepositoryRoot, "ignored.txt")));

        using var rebase_fixture = new TemporaryAgentGitRepository();
        rebase_fixture.RunGit("switch", "-qc", "feature/rebase-ignored");
        rebase_fixture.WriteFile(".gitignore", "ignored.txt\n");
        rebase_fixture.RunGit("add", ".gitignore");
        rebase_fixture.RunGit("commit", "-qm", "ignore local file");
        rebase_fixture.RunGit("switch", "dev");
        rebase_fixture.WriteFile("ignored.txt", "base data\n");
        rebase_fixture.RunGit("add", "ignored.txt");
        rebase_fixture.RunGit("commit", "-qm", "base file");
        rebase_fixture.RunGit("switch", "feature/rebase-ignored");
        rebase_fixture.WriteFile("ignored.txt", "local data\n");

        var rebase_result = await rebase_fixture.RunAgentGitAsync(
            rebase_fixture.RepositoryRoot,
            "rebase-base");

        Assert.AreEqual(ExitCodes.PolicyDenied, rebase_result.ExitCode, rebase_result.Error);
        StringAssert.Contains(rebase_result.Output, "ignored.txt");
        Assert.AreEqual(
            "local data\n",
            File.ReadAllText(Path.Combine(rebase_fixture.RepositoryRoot, "ignored.txt")));
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
