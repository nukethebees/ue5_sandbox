using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace AgentGit.Tests;

[TestClass]
public sealed class CommandLineTests
{
    [TestMethod]
    public void TryParse_rejects_unknown_commands_without_passthrough()
    {
        var parsed = CommandLine.TryParse(["exec", "status"], out var request, out var error);

        Assert.IsFalse(parsed);
        Assert.IsNull(request);
        StringAssert.Contains(error, "Unknown");
    }

    [TestMethod]
    public void TryParse_preserves_commit_message_as_one_value()
    {
        const string message = "message & whoami; $(touch marker)";

        var parsed = CommandLine.TryParse(["commit", "-m", message], out var request, out var error);

        Assert.IsTrue(parsed, error);
        Assert.AreEqual(message, ((CommitRequest)request!).Message);
    }

    [TestMethod]
    public void TryParse_allows_dry_run_only_for_mutations()
    {
        Assert.IsTrue(CommandLine.TryParse(["--dry-run", "add-all"], out var request, out _));
        Assert.IsInstanceOfType<AddAllRequest>(request);

        Assert.IsFalse(CommandLine.TryParse(["--dry-run", "status"], out _, out var error));
        StringAssert.Contains(error, "only valid for mutating");
    }

    [TestMethod]
    public void TryParse_policy_requires_targets_only_for_targeted_operations()
    {
        Assert.IsTrue(CommandLine.TryParse(["policy", "commit"], out _, out _));
        Assert.IsTrue(CommandLine.TryParse(["policy", "switch", "feature"], out _, out _));
        Assert.IsFalse(CommandLine.TryParse(["policy", "switch"], out _, out _));
        Assert.IsFalse(CommandLine.TryParse(["policy", "commit", "feature"], out _, out _));
    }

    [TestMethod]
    public void TryParse_integration_requires_explicit_authorization()
    {
        Assert.IsFalse(CommandLine.TryParse(["integrate"], out _, out var missing_error));
        StringAssert.Contains(missing_error, "--authorized");

        Assert.IsTrue(CommandLine.TryParse(
            ["integrate", "--authorized", "--keep-branch", "--tool-tests"],
            out var request,
            out var error), error);
        Assert.IsTrue(((IntegrateRequest)request!).Authorized);
        Assert.IsTrue(((IntegrateRequest)request).KeepBranch);
        Assert.IsTrue(((IntegrateRequest)request).ToolTests);
        Assert.IsFalse(CommandLine.TryParse(
            ["--dry-run", "integrate", "--authorized"],
            out _,
            out _));
    }

    [TestMethod]
    public void TryParse_accepts_only_argument_free_rebase_recovery_commands()
    {
        Assert.IsTrue(CommandLine.TryParse(["rebase-continue"], out var continue_request, out _));
        Assert.IsInstanceOfType<RebaseContinueRequest>(continue_request);
        Assert.IsTrue(CommandLine.TryParse(["--dry-run", "rebase-abort"], out var abort_request, out _));
        Assert.IsInstanceOfType<RebaseAbortRequest>(abort_request);

        Assert.IsFalse(CommandLine.TryParse(["rebase-continue", "dev"], out _, out _));
        Assert.IsFalse(CommandLine.TryParse(["rebase-abort", "--force"], out _, out _));
        Assert.IsFalse(CommandLine.TryParse(["rebase-skip"], out _, out _));
    }
}
