using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace AgentGit.Tests;

[TestClass]
public sealed class AgentGitAdversarialTests
{
    [TestMethod]
    public async Task Inherited_git_directory_cannot_redirect_operation_to_another_repository()
    {
        using var fixture = new TemporaryAgentGitRepository();
        fixture.RunGit("switch", "-qc", "feature/environment");
        fixture.WriteFile("change.txt", "change\n");
        var previous = Environment.GetEnvironmentVariable("GIT_DIR");
        Environment.SetEnvironmentVariable("GIT_DIR", Path.Combine(fixture.RepositoryRoot, "not-a-repository"));
        ApplicationResult result;
        try
        {
            result = await fixture.RunAgentGitAsync(fixture.RepositoryRoot, "add-all");
        }
        finally
        {
            Environment.SetEnvironmentVariable("GIT_DIR", previous);
        }

        Assert.AreEqual(ExitCodes.Success, result.ExitCode, result.Error);
        StringAssert.Contains(fixture.RunGit("diff", "--cached", "--name-only"), "change.txt");
    }

    [TestMethod]
    public async Task Configured_hooks_are_disabled_for_commit()
    {
        using var fixture = new TemporaryAgentGitRepository();
        fixture.RunGit("switch", "-qc", "feature/hooks");
        var hooks = Directory.CreateDirectory(Path.Combine(fixture.RepositoryRoot, ".hooks")).FullName;
        fixture.WriteFile(".hooks/post-commit", "#!/bin/sh\nprintf invoked > hook-marker\n");
        fixture.RunGit("config", "core.hooksPath", hooks);
        fixture.WriteFile("change.txt", "change\n");

        var add = await fixture.RunAgentGitAsync(fixture.RepositoryRoot, "add-all");
        var commit = await fixture.RunAgentGitAsync(fixture.RepositoryRoot, "commit", "-m", "safe commit");

        Assert.AreEqual(ExitCodes.Success, add.ExitCode, add.Error);
        Assert.AreEqual(ExitCodes.Success, commit.ExitCode, commit.Error);
        Assert.IsFalse(File.Exists(Path.Combine(fixture.RepositoryRoot, "hook-marker")));
    }

    [TestMethod]
    public async Task Unknown_executable_filter_and_merge_driver_fail_closed()
    {
        using var filter_fixture = new TemporaryAgentGitRepository();
        filter_fixture.RunGit("config", "filter.evil.clean", "arbitrary-command");
        var filter = await filter_fixture.RunAgentGitAsync(filter_fixture.RepositoryRoot, "status");
        Assert.AreEqual(ExitCodes.StateFailure, filter.ExitCode);
        StringAssert.Contains(filter.Error, "unsupported executable settings");

        using var merge_fixture = new TemporaryAgentGitRepository();
        merge_fixture.RunGit("config", "merge.evil.driver", "arbitrary-command %O %A %B");
        var merge = await merge_fixture.RunAgentGitAsync(merge_fixture.RepositoryRoot, "status");
        Assert.AreEqual(ExitCodes.StateFailure, merge.ExitCode);
        StringAssert.Contains(merge.Error, "unsupported executable settings");
    }

    [TestMethod]
    public async Task Local_config_include_fails_closed()
    {
        using var fixture = new TemporaryAgentGitRepository();
        fixture.WriteFile("attacker.gitconfig", "[filter \"evil\"]\nclean = arbitrary-command\n");
        fixture.RunGit("config", "include.path", Path.Combine(fixture.RepositoryRoot, "attacker.gitconfig"));

        var result = await fixture.RunAgentGitAsync(fixture.RepositoryRoot, "status");

        Assert.AreEqual(ExitCodes.StateFailure, result.ExitCode, result.Error);
        StringAssert.Contains(result.Error, "unsupported executable settings");
    }

    [TestMethod]
    public async Task Local_core_worktree_cannot_expand_the_trusted_repository_root()
    {
        using var fixture = new TemporaryAgentGitRepository();
        fixture.RunGit("config", "core.worktree", Path.GetDirectoryName(fixture.RepositoryRoot)!);

        var result = await fixture.RunAgentGitAsync(fixture.RepositoryRoot, "status");

        Assert.AreEqual(ExitCodes.StateFailure, result.ExitCode, result.Error);
        StringAssert.Contains(result.Error, "core.worktree");
    }

    [TestMethod]
    public async Task Symbolic_feature_ref_cannot_redirect_a_commit_to_protected_branch()
    {
        using var fixture = new TemporaryAgentGitRepository();
        fixture.RunGit("symbolic-ref", "refs/heads/feature/symbolic", "refs/heads/dev");
        fixture.RunGit("symbolic-ref", "HEAD", "refs/heads/feature/symbolic");
        fixture.WriteFile("change.txt", "change\n");

        var result = await fixture.RunAgentGitAsync(fixture.RepositoryRoot, "add-all");

        Assert.AreEqual(ExitCodes.StateFailure, result.ExitCode);
        StringAssert.Contains(result.Error, "is symbolic");
        Assert.AreEqual(
            fixture.RunGit("rev-parse", "dev").Trim(),
            fixture.RunGit("rev-parse", "feature/symbolic").Trim());
    }

    [TestMethod]
    public async Task Custom_lfs_transfer_and_credential_helpers_fail_closed()
    {
        using var transfer_fixture = new TemporaryAgentGitRepository();
        transfer_fixture.RunGit("config", "lfs.customtransfer.evil.path", "arbitrary-command");
        var transfer = await transfer_fixture.RunAgentGitAsync(transfer_fixture.RepositoryRoot, "status");
        Assert.AreEqual(ExitCodes.StateFailure, transfer.ExitCode);
        StringAssert.Contains(transfer.Error, "unsupported executable settings");

        using var credential_fixture = new TemporaryAgentGitRepository();
        credential_fixture.RunGit("config", "credential.helper", "!arbitrary-command");
        var credential = await credential_fixture.RunAgentGitAsync(credential_fixture.RepositoryRoot, "status");
        Assert.AreEqual(ExitCodes.StateFailure, credential.ExitCode);
        StringAssert.Contains(credential.Error, "unsupported executable settings");
    }

    [TestMethod]
    public async Task Legacy_grafts_cannot_falsify_branch_ancestry()
    {
        using var fixture = new TemporaryAgentGitRepository();
        var common_git_directory = fixture.RunGit(
            "rev-parse",
            "--path-format=absolute",
            "--git-common-dir").Trim();
        Directory.CreateDirectory(Path.Combine(common_git_directory, "info"));
        File.WriteAllText(
            Path.Combine(common_git_directory, "info", "grafts"),
            fixture.RunGit("rev-parse", "HEAD").Trim() + Environment.NewLine);

        var result = await fixture.RunAgentGitAsync(fixture.RepositoryRoot, "status");

        Assert.AreEqual(ExitCodes.StateFailure, result.ExitCode, result.Error);
        StringAssert.Contains(result.Error, "graft files");
    }

    [TestMethod]
    public async Task Malformed_and_unsupported_base_policy_prevent_all_repository_commands()
    {
        using var malformed_fixture = new TemporaryAgentGitRepository();
        malformed_fixture.WriteFile(".agent-git.json", "{ not json }");
        malformed_fixture.RunGit("add", ".agent-git.json");
        malformed_fixture.RunGit("commit", "-qm", "malformed policy");
        var malformed = await malformed_fixture.RunAgentGitAsync(malformed_fixture.RepositoryRoot, "status");
        Assert.AreEqual(ExitCodes.ConfigurationFailure, malformed.ExitCode);

        using var version_fixture = new TemporaryAgentGitRepository();
        version_fixture.WriteFile(
            ".agent-git.json",
            PolicyLoaderTests.ValidPolicy().Replace("\"version\": 1", "\"version\": 99", StringComparison.Ordinal));
        version_fixture.RunGit("add", ".agent-git.json");
        version_fixture.RunGit("commit", "-qm", "unsupported policy");
        var unsupported = await version_fixture.RunAgentGitAsync(version_fixture.RepositoryRoot, "branch-info");
        Assert.AreEqual(ExitCodes.ConfigurationFailure, unsupported.ExitCode);
    }

    [TestMethod]
    public async Task Missing_policy_and_non_repository_working_directory_fail_without_mutation()
    {
        using var missing_fixture = new TemporaryAgentGitRepository();
        missing_fixture.RunGit("rm", ".agent-git.json");
        missing_fixture.RunGit("commit", "-qm", "remove policy");

        var missing = await missing_fixture.RunAgentGitAsync(missing_fixture.RepositoryRoot, "status");
        Assert.AreEqual(ExitCodes.ConfigurationFailure, missing.ExitCode);
        StringAssert.Contains(missing.Error, "Unable to load policy");

        using var repository_fixture = new TemporaryAgentGitRepository();
        var outside = Directory.CreateDirectory(Path.Combine(repository_fixture.InstallRoot, "outside")).FullName;
        var not_repository = await repository_fixture.RunAgentGitAsync(outside, "status");
        Assert.AreEqual(ExitCodes.RepositoryFailure, not_repository.ExitCode);
    }

    [TestMethod]
    public async Task Dry_run_and_policy_inspection_never_execute_hooks_or_mutate_refs()
    {
        using var fixture = new TemporaryAgentGitRepository();
        var before = fixture.RunGit("show-ref");
        fixture.WriteFile("change.txt", "change\n");

        var dry_run = await fixture.RunAgentGitAsync(
            fixture.RepositoryRoot,
            "--dry-run",
            "switch-create",
            "feature/inspection");
        var policy = await fixture.RunAgentGitAsync(
            fixture.RepositoryRoot,
            "policy",
            "switch-create",
            "feature/inspection");

        Assert.AreEqual(ExitCodes.Success, dry_run.ExitCode, dry_run.Error);
        Assert.AreEqual(ExitCodes.Success, policy.ExitCode, policy.Error);
        Assert.AreEqual(before, fixture.RunGit("show-ref"));
        Assert.IsTrue(File.Exists(Path.Combine(fixture.RepositoryRoot, "change.txt")));
    }
}
