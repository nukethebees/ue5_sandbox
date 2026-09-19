using System.Text.Json;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace AgentGit.Tests;

[TestClass]
public sealed class AgentGitRebaseRecoveryTests
{
    [TestMethod]
    public async Task Owned_rebase_conflict_can_be_staged_and_continued()
    {
        using var fixture = new TemporaryAgentGitRepository();
        var setup = CreateConflictingRebase(fixture, "feature/recovery", conflict_count: 1);

        var start = await fixture.RunAgentGitAsync(setup.Worktree, "rebase-base");
        var status = await fixture.RunAgentGitAsync(setup.Worktree, "status");

        Assert.AreEqual(ExitCodes.GitFailure, start.ExitCode, start.Error);
        Assert.AreEqual(ExitCodes.Success, status.ExitCode, status.Error);
        StringAssert.Contains(status.Output, "Git operation: rebase");
        StringAssert.Contains(status.Output, "AgentGit rebase recovery: available");
        StringAssert.Contains(status.Output, $"Original branch: {setup.Branch}");
        StringAssert.Contains(status.Output, $"Base: dev @ {setup.BaseHead}");

        fixture.WriteFile("conflict-1.txt", "resolved\n", setup.Worktree);
        var add = await fixture.RunAgentGitAsync(setup.Worktree, "add", "conflict-1.txt");
        var resume = await fixture.RunAgentGitAsync(setup.Worktree, "rebase-continue");

        Assert.AreEqual(ExitCodes.Success, add.ExitCode, add.Error);
        Assert.AreEqual(ExitCodes.Success, resume.ExitCode, resume.Error);
        Assert.AreEqual(setup.Branch + "\n", fixture.RunGitAt(setup.Worktree, "branch", "--show-current"));
        fixture.RunGitAt(setup.Worktree, "merge-base", "--is-ancestor", setup.BaseHead, "HEAD");
        Assert.IsFalse(File.Exists(MarkerPath(fixture, setup.Worktree)));
        Assert.IsFalse(Directory.Exists(Path.Combine(GitDirectory(fixture, setup.Worktree), "rebase-merge")));
    }

    [TestMethod]
    public async Task Owned_rebase_retains_recovery_across_multiple_conflicts()
    {
        using var fixture = new TemporaryAgentGitRepository();
        var setup = CreateConflictingRebase(fixture, "feature/multiple", conflict_count: 2);
        Assert.AreEqual(
            ExitCodes.GitFailure,
            (await fixture.RunAgentGitAsync(setup.Worktree, "rebase-base")).ExitCode);

        fixture.WriteFile("conflict-1.txt", "resolved first\n", setup.Worktree);
        Assert.AreEqual(
            ExitCodes.Success,
            (await fixture.RunAgentGitAsync(setup.Worktree, "add-all")).ExitCode);
        var first_continue = await fixture.RunAgentGitAsync(setup.Worktree, "rebase-continue");
        var middle_status = await fixture.RunAgentGitAsync(setup.Worktree, "status");

        Assert.AreEqual(ExitCodes.GitFailure, first_continue.ExitCode, first_continue.Error);
        StringAssert.Contains(middle_status.Output, "AgentGit rebase recovery: available");
        Assert.IsTrue(File.Exists(MarkerPath(fixture, setup.Worktree)));

        fixture.WriteFile("conflict-2.txt", "resolved second\n", setup.Worktree);
        Assert.AreEqual(
            ExitCodes.Success,
            (await fixture.RunAgentGitAsync(setup.Worktree, "add", "conflict-2.txt")).ExitCode);
        var second_continue = await fixture.RunAgentGitAsync(setup.Worktree, "rebase-continue");

        Assert.AreEqual(ExitCodes.Success, second_continue.ExitCode, second_continue.Error);
        Assert.AreEqual(setup.Branch + "\n", fixture.RunGitAt(setup.Worktree, "branch", "--show-current"));
        Assert.IsFalse(File.Exists(MarkerPath(fixture, setup.Worktree)));
    }

    [TestMethod]
    public async Task Owned_rebase_abort_restores_original_branch_and_head()
    {
        using var fixture = new TemporaryAgentGitRepository();
        var setup = CreateConflictingRebase(fixture, "feature/abort", conflict_count: 1);
        Assert.AreEqual(
            ExitCodes.GitFailure,
            (await fixture.RunAgentGitAsync(setup.Worktree, "rebase-base")).ExitCode);

        var abort = await fixture.RunAgentGitAsync(setup.Worktree, "rebase-abort");

        Assert.AreEqual(ExitCodes.Success, abort.ExitCode, abort.Error);
        Assert.AreEqual(setup.Branch + "\n", fixture.RunGitAt(setup.Worktree, "branch", "--show-current"));
        Assert.AreEqual(setup.OriginalHead, fixture.RunGitAt(setup.Worktree, "rev-parse", "HEAD").Trim());
        Assert.IsFalse(File.Exists(MarkerPath(fixture, setup.Worktree)));
    }

    [TestMethod]
    public async Task Externally_started_rebase_is_never_adopted()
    {
        using var fixture = new TemporaryAgentGitRepository();
        var setup = CreateConflictingRebase(fixture, "feature/external", conflict_count: 1);
        var raw_start = fixture.RunGitAllowFailureAt(setup.Worktree, "rebase", "dev");
        Assert.AreNotEqual(0, raw_start.ExitCode);
        fixture.WriteFile("conflict-1.txt", "resolved\n", setup.Worktree);

        var add = await fixture.RunAgentGitAsync(setup.Worktree, "add", "conflict-1.txt");
        var resume = await fixture.RunAgentGitAsync(setup.Worktree, "rebase-continue");
        var abort = await fixture.RunAgentGitAsync(setup.Worktree, "rebase-abort");
        var status = await fixture.RunAgentGitAsync(setup.Worktree, "status");

        Assert.AreEqual(ExitCodes.PolicyDenied, add.ExitCode, add.Error);
        Assert.AreEqual(ExitCodes.PolicyDenied, resume.ExitCode, resume.Error);
        Assert.AreEqual(ExitCodes.PolicyDenied, abort.ExitCode, abort.Error);
        StringAssert.Contains(status.Output, "AgentGit rebase recovery: unavailable (not AgentGit-owned)");
        fixture.RunGitAt(setup.Worktree, "rebase", "--abort");
    }

    [TestMethod]
    public async Task Forged_and_tampered_recovery_metadata_fails_closed()
    {
        using var forged_fixture = new TemporaryAgentGitRepository();
        var forged_setup = CreateConflictingRebase(forged_fixture, "feature/forged", conflict_count: 1);
        Assert.AreNotEqual(
            0,
            forged_fixture.RunGitAllowFailureAt(forged_setup.Worktree, "rebase", "dev").ExitCode);
        var forged_marker = new RebaseRecoveryMarker(
            1,
            "active",
            new string('A', 64),
            "wrong/repository",
            forged_setup.Branch,
            forged_setup.OriginalHead,
            "dev",
            forged_setup.BaseHead,
            forged_setup.BaseHead,
            new string('B', 64));
        WriteMarker(forged_fixture, forged_setup.Worktree, forged_marker);
        File.WriteAllText(
            Path.Combine(GitDirectory(forged_fixture, forged_setup.Worktree), "rebase-merge", RebaseRecoveryStore.SentinelFileName),
            forged_marker.Token + Environment.NewLine);

        var forged_abort = await forged_fixture.RunAgentGitAsync(forged_setup.Worktree, "rebase-abort");
        Assert.AreEqual(ExitCodes.PolicyDenied, forged_abort.ExitCode, forged_abort.Error);
        forged_fixture.RunGitAt(forged_setup.Worktree, "rebase", "--abort");

        using var tampered_fixture = new TemporaryAgentGitRepository();
        var tampered_setup = CreateConflictingRebase(tampered_fixture, "feature/tampered", conflict_count: 1);
        Assert.AreEqual(
            ExitCodes.GitFailure,
            (await tampered_fixture.RunAgentGitAsync(tampered_setup.Worktree, "rebase-base")).ExitCode);
        File.WriteAllText(MarkerPath(tampered_fixture, tampered_setup.Worktree), "{ malformed json");

        var tampered_continue = await tampered_fixture.RunAgentGitAsync(
            tampered_setup.Worktree,
            "rebase-continue");
        Assert.AreEqual(ExitCodes.PolicyDenied, tampered_continue.ExitCode, tampered_continue.Error);
        File.WriteAllText(
            MarkerPath(tampered_fixture, tampered_setup.Worktree),
            "{\"version\":1,\"phase\":\"active\"}");
        var missing_fields_continue = await tampered_fixture.RunAgentGitAsync(
            tampered_setup.Worktree,
            "rebase-continue");
        Assert.AreEqual(ExitCodes.PolicyDenied, missing_fields_continue.ExitCode, missing_fields_continue.Error);
        tampered_fixture.RunGitAt(tampered_setup.Worktree, "rebase", "--abort");
    }

    [TestMethod]
    public async Task Changed_original_branch_ref_fails_closed()
    {
        using var fixture = new TemporaryAgentGitRepository();
        var setup = CreateConflictingRebase(fixture, "feature/ref-moved", conflict_count: 1);
        Assert.AreEqual(
            ExitCodes.GitFailure,
            (await fixture.RunAgentGitAsync(setup.Worktree, "rebase-base")).ExitCode);
        fixture.RunGit("update-ref", $"refs/heads/{setup.Branch}", setup.BaseHead, setup.OriginalHead);

        var status = await fixture.RunAgentGitAsync(setup.Worktree, "status");
        var resume = await fixture.RunAgentGitAsync(setup.Worktree, "rebase-continue");
        var abort = await fixture.RunAgentGitAsync(setup.Worktree, "rebase-abort");

        StringAssert.Contains(status.Output, "AgentGit rebase recovery: unavailable (not AgentGit-owned)");
        Assert.AreEqual(ExitCodes.PolicyDenied, resume.ExitCode, resume.Error);
        Assert.AreEqual(ExitCodes.PolicyDenied, abort.ExitCode, abort.Error);
    }

    [TestMethod]
    public async Task Redirected_rebase_administrative_directory_fails_closed()
    {
        using var fixture = new TemporaryAgentGitRepository();
        var setup = CreateConflictingRebase(fixture, "feature/redirected", conflict_count: 1);
        Assert.AreEqual(
            ExitCodes.GitFailure,
            (await fixture.RunAgentGitAsync(setup.Worktree, "rebase-base")).ExitCode);
        var rebase_directory = Path.Combine(GitDirectory(fixture, setup.Worktree), "rebase-merge");
        var outside = Path.Combine(fixture.InstallRoot, "redirected-rebase");
        Directory.Move(rebase_directory, outside);
        AgentGitAdversarialTests.CreateDirectoryLink(rebase_directory, outside);
        try
        {
            var abort = await fixture.RunAgentGitAsync(setup.Worktree, "rebase-abort");

            Assert.AreEqual(ExitCodes.StateFailure, abort.ExitCode, abort.Error);
            StringAssert.Contains(abort.Error, "symbolic link or reparse point");
        }
        finally
        {
            AgentGitAdversarialTests.RemoveDirectoryLink(rebase_directory);
        }
    }

    [TestMethod]
    public async Task Stale_marker_does_not_authorize_or_block_a_new_owned_rebase()
    {
        using var fixture = new TemporaryAgentGitRepository();
        var setup = CreateConflictingRebase(fixture, "feature/stale", conflict_count: 1);
        File.WriteAllText(MarkerPath(fixture, setup.Worktree), "{ stale marker }");

        var stale_status = await fixture.RunAgentGitAsync(setup.Worktree, "status");
        var stale_abort = await fixture.RunAgentGitAsync(setup.Worktree, "rebase-abort");
        var start = await fixture.RunAgentGitAsync(setup.Worktree, "rebase-base");
        var status = await fixture.RunAgentGitAsync(setup.Worktree, "status");

        StringAssert.Contains(stale_status.Output, "AgentGit rebase recovery: unavailable (stale metadata)");
        Assert.AreEqual(ExitCodes.PolicyDenied, stale_abort.ExitCode, stale_abort.Error);
        Assert.AreEqual(ExitCodes.GitFailure, start.ExitCode, start.Error);
        StringAssert.Contains(status.Output, "AgentGit rebase recovery: available");
        Assert.AreEqual(
            ExitCodes.Success,
            (await fixture.RunAgentGitAsync(setup.Worktree, "rebase-abort")).ExitCode);
    }

    [TestMethod]
    public async Task Only_recovery_operations_are_opened_during_owned_rebase()
    {
        using var fixture = new TemporaryAgentGitRepository();
        var setup = CreateConflictingRebase(fixture, "feature/minimal-surface", conflict_count: 1);
        fixture.RunGit("branch", "feature/other", "dev");
        Assert.AreEqual(
            ExitCodes.GitFailure,
            (await fixture.RunAgentGitAsync(setup.Worktree, "rebase-base")).ExitCode);

        foreach (var command in new[]
                 {
                     new[] { "commit", "-m", "forbidden" },
                     new[] { "switch", "feature/other" },
                     new[] { "switch-create", "feature/new" },
                     new[] { "branch-delete", "feature/other" },
                     new[] { "rebase-base" },
                 })
        {
            var result = await fixture.RunAgentGitAsync(setup.Worktree, command);
            Assert.AreEqual(ExitCodes.PolicyDenied, result.ExitCode, string.Join(' ', command) + result.Error);
        }

        Assert.AreEqual(
            ExitCodes.Success,
            (await fixture.RunAgentGitAsync(setup.Worktree, "rebase-abort")).ExitCode);
    }

    [TestMethod]
    public async Task Base_or_policy_movement_blocks_continue_but_not_abort()
    {
        using var fixture = new TemporaryAgentGitRepository();
        var setup = CreateConflictingRebase(fixture, "feature/policy-drift", conflict_count: 1);
        Assert.AreEqual(
            ExitCodes.GitFailure,
            (await fixture.RunAgentGitAsync(setup.Worktree, "rebase-base")).ExitCode);
        fixture.WriteFile("base-moved.txt", "moved\n");
        fixture.RunGit("add", "base-moved.txt");
        fixture.RunGit("commit", "-qm", "move base during conflict");

        var status = await fixture.RunAgentGitAsync(setup.Worktree, "status");
        var add = await fixture.RunAgentGitAsync(setup.Worktree, "add-all");
        var resume = await fixture.RunAgentGitAsync(setup.Worktree, "rebase-continue");
        var abort = await fixture.RunAgentGitAsync(setup.Worktree, "rebase-abort");

        StringAssert.Contains(status.Output, "AgentGit rebase recovery: abort only");
        Assert.AreEqual(ExitCodes.PolicyDenied, add.ExitCode, add.Error);
        Assert.AreEqual(ExitCodes.PolicyDenied, resume.ExitCode, resume.Error);
        Assert.AreEqual(ExitCodes.Success, abort.ExitCode, abort.Error);
        Assert.AreEqual(setup.OriginalHead, fixture.RunGitAt(setup.Worktree, "rev-parse", "HEAD").Trim());
    }

    [TestMethod]
    public async Task Continue_uses_fixed_non_interactive_editor_and_disables_hooks()
    {
        using var fixture = new TemporaryAgentGitRepository();
        var setup = CreateConflictingRebase(fixture, "feature/non-interactive", conflict_count: 1);
        var editor = Path.Combine(fixture.InstallRoot, "hostile-editor.cmd");
        File.WriteAllText(editor, "@echo invoked>editor-invoked\r\n");
        var hooks = Directory.CreateDirectory(Path.Combine(fixture.InstallRoot, "hooks")).FullName;
        File.WriteAllText(
            Path.Combine(hooks, "commit-msg"),
            "#!/bin/sh\nprintf invoked > hook-invoked\n");
        fixture.RunGitAt(setup.Worktree, "config", "core.editor", editor);
        fixture.RunGitAt(setup.Worktree, "config", "core.hooksPath", hooks);
        Assert.AreEqual(
            ExitCodes.GitFailure,
            (await fixture.RunAgentGitAsync(setup.Worktree, "rebase-base")).ExitCode);
        fixture.WriteFile("conflict-1.txt", "resolved\n", setup.Worktree);
        Assert.AreEqual(
            ExitCodes.Success,
            (await fixture.RunAgentGitAsync(setup.Worktree, "add", "conflict-1.txt")).ExitCode);

        var resume = await fixture.RunAgentGitAsync(setup.Worktree, "rebase-continue");

        Assert.AreEqual(ExitCodes.Success, resume.ExitCode, resume.Error);
        Assert.IsFalse(File.Exists(Path.Combine(setup.Worktree, "editor-invoked")));
        Assert.IsFalse(File.Exists(Path.Combine(setup.Worktree, "hook-invoked")));
    }

    private static ConflictSetup CreateConflictingRebase(
        TemporaryAgentGitRepository fixture,
        string branch,
        int conflict_count)
    {
        for (var index = 1; index <= conflict_count; ++index)
        {
            fixture.WriteFile($"conflict-{index}.txt", "initial\n");
        }

        fixture.RunGit("add", "--all");
        fixture.RunGit("commit", "-qm", "add conflict inputs");
        var worktree = fixture.CreateWorktree(branch, "dev1");
        for (var index = 1; index <= conflict_count; ++index)
        {
            fixture.WriteFile($"conflict-{index}.txt", $"feature {index}\n", worktree);
            fixture.RunGitAt(worktree, "add", $"conflict-{index}.txt");
            fixture.RunGitAt(worktree, "commit", "-qm", $"feature conflict {index}");
        }

        var original_head = fixture.RunGitAt(worktree, "rev-parse", "HEAD").Trim();
        for (var index = 1; index <= conflict_count; ++index)
        {
            fixture.WriteFile($"conflict-{index}.txt", $"base {index}\n");
        }

        fixture.RunGit("add", "--all");
        fixture.RunGit("commit", "-qm", "base conflicts");
        var base_head = fixture.RunGit("rev-parse", "dev").Trim();
        return new ConflictSetup(branch, worktree, original_head, base_head);
    }

    private static string GitDirectory(TemporaryAgentGitRepository fixture, string worktree)
    {
        return Path.GetFullPath(fixture.RunGitAt(
            worktree,
            "rev-parse",
            "--path-format=absolute",
            "--git-dir").Trim());
    }

    private static string MarkerPath(TemporaryAgentGitRepository fixture, string worktree)
    {
        return Path.Combine(GitDirectory(fixture, worktree), RebaseRecoveryStore.MarkerFileName);
    }

    private static void WriteMarker(
        TemporaryAgentGitRepository fixture,
        string worktree,
        RebaseRecoveryMarker marker)
    {
        var options = new JsonSerializerOptions
        {
            PropertyNamingPolicy = JsonNamingPolicy.CamelCase,
        };
        File.WriteAllText(MarkerPath(fixture, worktree), JsonSerializer.Serialize(marker, options));
    }

    private sealed record ConflictSetup(
        string Branch,
        string Worktree,
        string OriginalHead,
        string BaseHead);
}
