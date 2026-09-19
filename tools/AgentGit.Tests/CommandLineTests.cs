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
}
