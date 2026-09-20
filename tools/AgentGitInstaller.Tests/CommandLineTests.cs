using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace AgentGitInstaller.Tests;

[TestClass]
public sealed class CommandLineTests
{
    [TestMethod]
    public void Parses_required_arguments_and_defaults()
    {
        Assert.IsTrue(CommandLine.TryParse(
            ["--source-root", "source", "--repository", "repository", "--base-branch", "dev"],
            out var request,
            out var show_help,
            out var error),
            error);

        Assert.IsFalse(show_help);
        Assert.IsNotNull(request);
        Assert.AreEqual(".agent-git.json", request.PolicyPath);
        Assert.IsNull(request.InstallRoot);
        Assert.IsFalse(request.TestSkipValidation);
    }

    [TestMethod]
    public void Rejects_unknown_duplicate_and_incomplete_options()
    {
        AssertParseFailure(["--unknown"], "Unknown option");
        AssertParseFailure(
            ["--source-root", "a", "--source-root", "b", "--repository", "r", "--base-branch", "dev"],
            "more than once");
        AssertParseFailure(["--source-root", "a", "--repository", "r"], "required");
        AssertParseFailure(["--source-root"], "requires a non-empty value");
    }

    private static void AssertParseFailure(string[] arguments, string message)
    {
        Assert.IsFalse(CommandLine.TryParse(arguments, out _, out _, out var error));
        StringAssert.Contains(error, message);
    }
}
