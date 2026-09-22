using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace AgentGit.Tests;

[TestClass]
public sealed class ProcessBudgetTests
{
    [TestMethod]
    public async Task Status_stays_within_git_process_budget()
    {
        using var fixture = new TemporaryAgentGitRepository();
        var runner = new CountingProcessRunner();

        var result = await fixture.RunAgentGitAsync(
            fixture.RepositoryRoot,
            runner,
            CancellationToken.None,
            "status");

        Assert.AreEqual(ExitCodes.Success, result.ExitCode, result.Error);
        Assert.IsTrue(runner.Count <= 11, runner.Describe());
    }

    [TestMethod]
    public async Task Commit_stays_within_git_process_budget()
    {
        using var fixture = new TemporaryAgentGitRepository();
        var feature = fixture.CreateWorktree("feature/process-budget", "process-budget");
        fixture.WriteFile("budget.txt", "content\n", feature);
        fixture.RunGitAt(feature, "add", "--", "budget.txt");
        var runner = new CountingProcessRunner();

        var result = await fixture.RunAgentGitAsync(
            feature,
            runner,
            CancellationToken.None,
            "commit",
            "-m",
            "process budget");

        Assert.AreEqual(ExitCodes.Success, result.ExitCode, result.Error);
        Assert.IsTrue(runner.Count <= 20, runner.Describe());
    }
}

internal sealed class CountingProcessRunner : IProcessRunner
{
    private readonly ProcessRunner inner = new();
    private readonly List<string> commands = [];
    private readonly object sync = new();

    public int Count
    {
        get
        {
            lock (sync)
            {
                return commands.Count;
            }
        }
    }

    public async Task<ProcessResult> RunAsync(
        ProcessRequest request,
        CancellationToken cancellation_token)
    {
        lock (sync)
        {
            commands.Add(FindCommand(request.Arguments));
        }

        return await inner.RunAsync(request, cancellation_token);
    }

    public string Describe()
    {
        lock (sync)
        {
            var histogram = commands
                .GroupBy(command => command, StringComparer.Ordinal)
                .OrderBy(group => group.Key, StringComparer.Ordinal)
                .Select(group => $"{group.Key}={group.Count()}");
            return $"Git process count: {commands.Count}. Commands: {string.Join(", ", histogram)}";
        }
    }

    private static string FindCommand(IReadOnlyList<string> arguments)
    {
        for (var index = 0; index < arguments.Count; ++index)
        {
            if (arguments[index] is "--no-pager" or "--literal-pathspecs")
            {
                continue;
            }

            if (arguments[index] == "-c" && index + 1 < arguments.Count)
            {
                ++index;
                continue;
            }

            return arguments[index];
        }

        return "<missing>";
    }
}
