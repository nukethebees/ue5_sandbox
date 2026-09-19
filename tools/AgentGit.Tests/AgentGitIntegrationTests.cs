using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace AgentGit.Tests;

[TestClass]
public sealed class AgentGitIntegrationTests
{
    [TestMethod]
    public async Task Discovery_works_from_subdirectory_and_reports_feature_policy()
    {
        using var fixture = new TemporaryAgentGitRepository();
        fixture.RunGit("switch", "-qc", "feature/discovery");
        var subdirectory = Directory.CreateDirectory(Path.Combine(fixture.RepositoryRoot, "Source", "Nested")).FullName;

        var result = await fixture.RunAgentGitAsync(subdirectory, "policy", "commit");

        Assert.AreEqual(ExitCodes.Success, result.ExitCode, result.Error);
        StringAssert.Contains(result.Output, "Current branch: feature/discovery");
        StringAssert.Contains(result.Output, "Classification: feature");
        StringAssert.Contains(result.Output, "Decision: ALLOW");
    }

    [TestMethod]
    public async Task Discovery_allows_enabled_but_absent_worktree_configuration()
    {
        using var fixture = new TemporaryAgentGitRepository();
        fixture.RunGit("config", "extensions.worktreeConfig", "true");

        var result = await fixture.RunAgentGitAsync(fixture.RepositoryRoot, "status");

        Assert.AreEqual(ExitCodes.Success, result.ExitCode, result.Error);
    }

    [TestMethod]
    public async Task Add_and_commit_are_denied_on_protected_and_workspace_branches()
    {
        using var fixture = new TemporaryAgentGitRepository();
        fixture.WriteFile("protected.txt", "change\n");

        var protected_add = await fixture.RunAgentGitAsync(fixture.RepositoryRoot, "add-all");
        var protected_commit = await fixture.RunAgentGitAsync(fixture.RepositoryRoot, "commit", "-m", "forbidden");

        Assert.AreEqual(ExitCodes.PolicyDenied, protected_add.ExitCode);
        Assert.AreEqual(ExitCodes.PolicyDenied, protected_commit.ExitCode);
        StringAssert.Contains(protected_commit.Output, "protected branch 'dev'");

        var workspace = fixture.CreateWorktree("dev1");
        fixture.WriteFile("workspace.txt", "change\n", workspace);
        var workspace_add = await fixture.RunAgentGitAsync(workspace, "add-all");
        Assert.AreEqual(ExitCodes.PolicyDenied, workspace_add.ExitCode);
        StringAssert.Contains(workspace_add.Output, "workspace branch 'dev1'");
    }

    [TestMethod]
    public async Task Switch_create_carries_changes_then_add_and_commit_preserve_metacharacters_as_data()
    {
        using var fixture = new TemporaryAgentGitRepository();
        const string file_name = "odd & name.txt";
        const string message = "literal & whoami; $(touch escaped-marker)";
        fixture.WriteFile(file_name, "content\n");

        var create = await fixture.RunAgentGitAsync(fixture.RepositoryRoot, "switch-create", "feature/odd-input");
        var add = await fixture.RunAgentGitAsync(fixture.RepositoryRoot, "add", file_name);
        var commit = await fixture.RunAgentGitAsync(fixture.RepositoryRoot, "commit", "-m", message);

        Assert.AreEqual(ExitCodes.Success, create.ExitCode, create.Error);
        Assert.AreEqual(ExitCodes.Success, add.ExitCode, add.Error);
        Assert.AreEqual(ExitCodes.Success, commit.ExitCode, commit.Error);
        Assert.AreEqual(message + "\n", fixture.RunGit("log", "-1", "--format=%s"));
        Assert.IsFalse(File.Exists(Path.Combine(fixture.RepositoryRoot, "escaped-marker")));
    }

    [TestMethod]
    public async Task Switch_requires_clean_tree_but_allows_return_to_own_home_branch()
    {
        using var fixture = new TemporaryAgentGitRepository();
        var workspace = fixture.CreateWorktree("feature/switch", "dev1");
        fixture.RunGit("branch", "dev1", "dev");
        fixture.WriteFile("dirty.txt", "dirty\n", workspace);

        var dirty = await fixture.RunAgentGitAsync(workspace, "switch", "dev1");
        Assert.AreEqual(ExitCodes.PolicyDenied, dirty.ExitCode);
        StringAssert.Contains(dirty.Output, "requires a clean");

        File.Delete(Path.Combine(workspace, "dirty.txt"));
        var clean = await fixture.RunAgentGitAsync(workspace, "switch", "dev1");
        Assert.AreEqual(ExitCodes.Success, clean.ExitCode, clean.Error);
        Assert.AreEqual("dev1\n", fixture.RunGitAt(workspace, "branch", "--show-current"));
    }

    [TestMethod]
    public async Task Feature_worktree_policy_edit_does_not_change_protected_ref_policy()
    {
        using var fixture = new TemporaryAgentGitRepository();
        fixture.RunGit("switch", "-qc", "feature/policy-tamper");
        var permissive = PolicyLoaderTests.ValidPolicy().Replace(
            "\"allowedCurrentGroups\": [\"feature\"]",
            "\"allowedCurrentGroups\": [\"feature\", \"protected\"]",
            StringComparison.Ordinal);
        fixture.WriteFile(".agent-git.json", permissive);
        fixture.RunGit("add", ".agent-git.json");
        fixture.RunGit("commit", "-qm", "tamper policy");

        var result = await fixture.RunAgentGitAsync(fixture.RepositoryRoot, "policy", "commit");

        Assert.AreEqual(ExitCodes.Success, result.ExitCode, result.Error);
        StringAssert.Contains(result.Output, "Decision: ALLOW");
        var branch_info = await fixture.RunAgentGitAsync(fixture.RepositoryRoot, "branch-info");
        StringAssert.Contains(branch_info.Output, "Policy commit:");
        Assert.AreNotEqual(
            fixture.RunGit("rev-parse", "HEAD").Trim(),
            fixture.RunGit("rev-parse", "dev").Trim());
    }

    [TestMethod]
    public async Task Dry_run_evaluates_without_mutating()
    {
        using var fixture = new TemporaryAgentGitRepository();

        var result = await fixture.RunAgentGitAsync(
            fixture.RepositoryRoot,
            "--dry-run",
            "switch-create",
            "feature/dry-run");

        Assert.AreEqual(ExitCodes.Success, result.ExitCode, result.Error);
        StringAssert.Contains(result.Output, "Dry run: no mutation executed");
        Assert.AreEqual("dev\n", fixture.RunGit("branch", "--show-current"));
        Assert.AreEqual(string.Empty, fixture.RunGit("branch", "--list", "feature/dry-run"));
    }
}
