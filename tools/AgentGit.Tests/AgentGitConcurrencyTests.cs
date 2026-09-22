using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace AgentGit.Tests;

[TestClass]
[DoNotParallelize]
public sealed class AgentGitConcurrencyTests
{
    [TestMethod]
    public async Task Independent_worktrees_commit_concurrently()
    {
        using var fixture = new TemporaryAgentGitRepository();
        var first = fixture.CreateWorktree("concurrency-first", "concurrency-first");
        var second = fixture.CreateWorktree("concurrency-second", "concurrency-second");
        fixture.WriteFile("first.txt", "first\n", first);
        fixture.WriteFile("second.txt", "second\n", second);

        Assert.AreEqual(ExitCodes.Success, (await fixture.RunAgentGitAsync(first, "add-all")).ExitCode);

        var pausing_runner = new PausingCommitRunner();
        var first_commit = fixture.RunAgentGitAsync(
            first,
            pausing_runner,
            CancellationToken.None,
            "commit",
            "-m",
            "first commit");
        await pausing_runner.WaitForPauseAsync();

        var second_commit = CommitAsync(fixture, second, "second commit");
        var completed = await Task.WhenAny(second_commit, Task.Delay(TimeSpan.FromSeconds(5)));
        Assert.AreSame(second_commit, completed, "A commit in another worktree blocked this worktree.");
        Assert.AreEqual(ExitCodes.Success, (await second_commit).ExitCode);

        pausing_runner.Release();
        Assert.AreEqual(ExitCodes.Success, (await first_commit).ExitCode);
    }

    [TestMethod]
    public async Task Independent_worktrees_inspect_concurrently()
    {
        using var fixture = new TemporaryAgentGitRepository();
        var first = fixture.CreateWorktree("inspection-first", "inspection-first");
        var second = fixture.CreateWorktree("inspection-second", "inspection-second");

        var results = await Task.WhenAll(
            fixture.RunAgentGitAsync(first, "status"),
            fixture.RunAgentGitAsync(second, "branch-info"));

        Assert.AreEqual(ExitCodes.Success, results[0].ExitCode);
        Assert.AreEqual(ExitCodes.Success, results[1].ExitCode);
    }

    [TestMethod]
    public async Task Legacy_agent_git_lock_handle_does_not_block_mutations()
    {
        using var fixture = new TemporaryAgentGitRepository();
        var worktree = fixture.CreateWorktree("legacy-lock", "legacy-lock");
        fixture.WriteFile("change.txt", "change\n", worktree);
        var common_git_directory = fixture.RunGitAt(
            worktree,
            "rev-parse",
            "--path-format=absolute",
            "--git-common-dir").Trim();
        var lock_path = Path.Combine(common_git_directory, "agent-git.lock");

        await using var held_lock = new FileStream(
            lock_path,
            FileMode.OpenOrCreate,
            FileAccess.ReadWrite,
            FileShare.None);

        Assert.AreEqual(ExitCodes.Success, (await fixture.RunAgentGitAsync(worktree, "add-all")).ExitCode);
        Assert.AreEqual(
            ExitCodes.Success,
            (await fixture.RunAgentGitAsync(worktree, "commit", "-m", "legacy lock ignored")).ExitCode);
    }

    [TestMethod]
    public async Task Cancelled_mutation_does_not_block_another_worktree()
    {
        using var fixture = new TemporaryAgentGitRepository();
        var first = fixture.CreateWorktree("cancelled-first", "cancelled-first");
        var second = fixture.CreateWorktree("cancelled-second", "cancelled-second");
        fixture.WriteFile("first.txt", "first\n", first);
        fixture.WriteFile("second.txt", "second\n", second);
        Assert.AreEqual(ExitCodes.Success, (await fixture.RunAgentGitAsync(first, "add-all")).ExitCode);

        var pausing_runner = new PausingCommitRunner();
        using var cancellation = new CancellationTokenSource();
        var cancelled_commit = fixture.RunAgentGitAsync(
            first,
            pausing_runner,
            cancellation.Token,
            "commit",
            "-m",
            "cancelled commit");
        await pausing_runner.WaitForPauseAsync();
        cancellation.Cancel();

        var cancelled = await cancelled_commit;
        Assert.AreEqual(ExitCodes.StateFailure, cancelled.ExitCode);
        Assert.AreEqual(ExitCodes.Success, (await CommitAsync(fixture, second, "second commit")).ExitCode);
    }

    private static async Task<ApplicationResult> CommitAsync(
        TemporaryAgentGitRepository fixture,
        string worktree,
        string message)
    {
        var add = await fixture.RunAgentGitAsync(worktree, "add-all");
        Assert.AreEqual(ExitCodes.Success, add.ExitCode);
        return await fixture.RunAgentGitAsync(worktree, "commit", "-m", message);
    }

    private sealed class PausingCommitRunner : IProcessRunner
    {
        private readonly ProcessRunner inner_ = new();
        private readonly TaskCompletionSource paused_ = new(TaskCreationOptions.RunContinuationsAsynchronously);
        private readonly TaskCompletionSource release_ = new(TaskCreationOptions.RunContinuationsAsynchronously);
        private int paused_once_;

        public async Task<ProcessResult> RunAsync(ProcessRequest request, CancellationToken cancellation_token)
        {
            if (request.Arguments.Contains("commit") && Interlocked.Exchange(ref paused_once_, 1) == 0)
            {
                paused_.TrySetResult();
                await release_.Task.WaitAsync(cancellation_token);
            }

            return await inner_.RunAsync(request, cancellation_token);
        }

        public async Task WaitForPauseAsync()
        {
            await paused_.Task.WaitAsync(TimeSpan.FromSeconds(10));
        }

        public void Release()
        {
            release_.TrySetResult();
        }
    }
}
