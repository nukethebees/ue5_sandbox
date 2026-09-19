using System.Diagnostics;
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
    public async Task Worktree_local_executable_configuration_fails_closed()
    {
        using var fixture = new TemporaryAgentGitRepository();
        fixture.RunGit("config", "extensions.worktreeConfig", "true");
        fixture.RunGit("config", "--worktree", "alias.status", "!arbitrary-command");

        var result = await fixture.RunAgentGitAsync(fixture.RepositoryRoot, "status");

        Assert.AreEqual(ExitCodes.StateFailure, result.ExitCode, result.Error);
        StringAssert.Contains(result.Error, "alias.status");
    }

    [TestMethod]
    public async Task Hidden_index_entries_cannot_bypass_cleanliness_checks()
    {
        using var assume_fixture = new TemporaryAgentGitRepository();
        assume_fixture.RunGit("update-index", "--assume-unchanged", "README.md");
        var assume = await assume_fixture.RunAgentGitAsync(assume_fixture.RepositoryRoot, "status");
        Assert.AreEqual(ExitCodes.StateFailure, assume.ExitCode, assume.Error);
        StringAssert.Contains(assume.Error, "Assume-unchanged");

        using var skip_fixture = new TemporaryAgentGitRepository();
        skip_fixture.RunGit("update-index", "--skip-worktree", "README.md");
        var skip = await skip_fixture.RunAgentGitAsync(skip_fixture.RepositoryRoot, "status");
        Assert.AreEqual(ExitCodes.StateFailure, skip.ExitCode, skip.Error);
        StringAssert.Contains(skip.Error, "skip-worktree");
    }

    [TestMethod]
    public async Task Executable_alias_diff_and_submodule_configuration_fail_closed()
    {
        foreach (var setting in new[]
                 {
                     (Name: "alias.status", Value: "!arbitrary-command"),
                     (Name: "core.excludesFile", Value: "outside-ignore"),
                     (Name: "core.fsmonitor", Value: "arbitrary-command"),
                     (Name: "diff.evil.textconv", Value: "arbitrary-command"),
                     (Name: "gpg.ssh.program", Value: "arbitrary-command"),
                     (Name: "submodule.example.update", Value: "!arbitrary-command"),
                 })
        {
            using var fixture = new TemporaryAgentGitRepository();
            fixture.RunGit("config", setting.Name, setting.Value);

            var result = await fixture.RunAgentGitAsync(fixture.RepositoryRoot, "status");

            Assert.AreEqual(ExitCodes.StateFailure, result.ExitCode, result.Error);
            Assert.IsTrue(
                result.Error.Contains(setting.Name, StringComparison.OrdinalIgnoreCase),
                result.Error);
        }
    }

    [TestMethod]
    public async Task Repository_origin_mismatch_fails_before_policy_evaluation()
    {
        using var fixture = new TemporaryAgentGitRepository();
        fixture.RunGit("remote", "set-url", "origin", "https://example.invalid/other.git");

        var result = await fixture.RunAgentGitAsync(fixture.RepositoryRoot, "status");

        Assert.AreEqual(ExitCodes.RepositoryFailure, result.ExitCode, result.Error);
        StringAssert.Contains(result.Error, "origin does not match");
    }

    [TestMethod]
    public async Task Add_operations_do_not_traverse_linked_directories()
    {
        using var fixture = new TemporaryAgentGitRepository();
        fixture.RunGit("switch", "-qc", "feature/linked-path");
        var outside = Directory.CreateDirectory(Path.Combine(fixture.InstallRoot, "outside")).FullName;
        File.WriteAllText(Path.Combine(outside, "outside.txt"), "outside\n");
        var link = Path.Combine(fixture.RepositoryRoot, "linked");
        try
        {
            CreateDirectoryLink(link, outside);
        }
        catch (Exception exception) when (exception is IOException or UnauthorizedAccessException)
        {
            Assert.Inconclusive($"Directory symbolic links are unavailable on this test host: {exception.Message}");
        }

        try
        {
            var selected = await fixture.RunAgentGitAsync(fixture.RepositoryRoot, "add", "linked");
            var all = await fixture.RunAgentGitAsync(fixture.RepositoryRoot, "add-all");

            Assert.AreEqual(ExitCodes.PolicyDenied, selected.ExitCode, selected.Error);
            StringAssert.Contains(selected.Output, "linked directory");
            Assert.AreEqual(ExitCodes.PolicyDenied, all.ExitCode, all.Error);
            StringAssert.Contains(all.Output, "linked directory");
            Assert.AreEqual(string.Empty, fixture.RunGit("diff", "--cached", "--name-only"));
        }
        finally
        {
            RemoveDirectoryLink(link);
        }
    }

    [TestMethod]
    public async Task Linked_git_administrative_directories_fail_closed()
    {
        using var fixture = new TemporaryAgentGitRepository();
        var common_git_directory = Path.GetFullPath(fixture.RunGit(
            "rev-parse",
            "--path-format=absolute",
            "--git-common-dir").Trim());
        var outside = Directory.CreateDirectory(Path.Combine(fixture.InstallRoot, "outside-refs")).FullName;
        var link = Path.Combine(common_git_directory, "refs", "heads", "linked");
        CreateDirectoryLink(link, outside);
        try
        {
            var result = await fixture.RunAgentGitAsync(fixture.RepositoryRoot, "status");

            Assert.AreEqual(ExitCodes.StateFailure, result.ExitCode, result.Error);
            StringAssert.Contains(result.Error, "Git administrative path");
        }
        finally
        {
            RemoveDirectoryLink(link);
        }
    }

    private static void CreateDirectoryLink(string link, string target)
    {
        if (!OperatingSystem.IsWindows())
        {
            Directory.CreateSymbolicLink(link, target);
            return;
        }

        var start_info = new ProcessStartInfo
        {
            FileName = "cmd.exe",
            UseShellExecute = false,
            RedirectStandardOutput = true,
            RedirectStandardError = true,
            CreateNoWindow = true,
        };
        foreach (var argument in new[] { "/d", "/c", "mklink", "/J", link, target })
        {
            start_info.ArgumentList.Add(argument);
        }

        using var process = Process.Start(start_info) ?? throw new IOException("Unable to start junction test setup.");
        var output = process.StandardOutput.ReadToEnd();
        var error = process.StandardError.ReadToEnd();
        process.WaitForExit();
        if (process.ExitCode != 0)
        {
            throw new IOException($"Unable to create test junction: {output}{error}");
        }
    }

    private static void RemoveDirectoryLink(string link)
    {
        if (!OperatingSystem.IsWindows())
        {
            Directory.Delete(link);
            return;
        }

        var start_info = new ProcessStartInfo
        {
            FileName = "cmd.exe",
            UseShellExecute = false,
            RedirectStandardOutput = true,
            RedirectStandardError = true,
            CreateNoWindow = true,
        };
        foreach (var argument in new[] { "/d", "/c", "rmdir", link })
        {
            start_info.ArgumentList.Add(argument);
        }

        using var process = Process.Start(start_info) ?? throw new IOException("Unable to start junction cleanup.");
        process.WaitForExit();
        if (process.ExitCode != 0)
        {
            throw new IOException($"Unable to remove test junction: {process.StandardError.ReadToEnd()}");
        }
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
    public async Task Pinned_lfs_filter_overrides_repository_commands_and_stages_pointer()
    {
        using var fixture = new TemporaryAgentGitRepository(include_lfs: true);
        fixture.RunGit("switch", "-qc", "feature/lfs");
        fixture.RunGit("config", "filter.lfs.clean", "false");
        fixture.RunGit("config", "filter.lfs.process", "false");
        fixture.WriteFile(".gitattributes", "*.bin filter=lfs diff=lfs merge=lfs -text\n");
        fixture.WriteFile("asset.bin", "agent-git lfs payload\n");

        var result = await fixture.RunAgentGitAsync(
            fixture.RepositoryRoot,
            "add",
            ".gitattributes",
            "asset.bin");

        if (result.ExitCode == ExitCodes.GitFailure &&
            result.Error.Contains("sh (", StringComparison.Ordinal) &&
            result.Error.Contains("Win32 error 5", StringComparison.Ordinal))
        {
            Assert.Inconclusive(
                "The host sandbox blocked Git-for-Windows from starting its fixed Git LFS filter process.");
        }

        Assert.AreEqual(ExitCodes.Success, result.ExitCode, result.Error);
        var staged_blob = fixture.RunGit("show", ":asset.bin");
        StringAssert.StartsWith(staged_blob, "version https://git-lfs.github.com/spec/v1\n");
        StringAssert.Contains(staged_blob, "oid sha256:");
        StringAssert.Contains(staged_blob, "size 22");
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

        using var oversized_fixture = new TemporaryAgentGitRepository();
        oversized_fixture.WriteFile(
            ".agent-git.json",
            PolicyLoaderTests.ValidPolicy() + new string(' ', 64 * 1024));
        oversized_fixture.RunGit("add", ".agent-git.json");
        oversized_fixture.RunGit("commit", "-qm", "oversized policy");
        var oversized = await oversized_fixture.RunAgentGitAsync(oversized_fixture.RepositoryRoot, "status");
        Assert.AreEqual(ExitCodes.ConfigurationFailure, oversized.ExitCode);
        StringAssert.Contains(oversized.Error, "64 KiB safety limit");
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
    public async Task Missing_base_branch_prevents_policy_loading_and_all_mutations()
    {
        using var fixture = new TemporaryAgentGitRepository();
        fixture.RunGit("switch", "-qc", "feature/missing-base");
        fixture.RunGit("branch", "-d", "dev");
        fixture.WriteFile("change.txt", "change\n");

        var status = await fixture.RunAgentGitAsync(fixture.RepositoryRoot, "status");
        var add = await fixture.RunAgentGitAsync(fixture.RepositoryRoot, "add-all");

        Assert.AreEqual(ExitCodes.ConfigurationFailure, status.ExitCode, status.Error);
        Assert.AreEqual(ExitCodes.ConfigurationFailure, add.ExitCode, add.Error);
        Assert.AreEqual(string.Empty, fixture.RunGit("diff", "--cached", "--name-only"));
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
