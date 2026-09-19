using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace AgentGit.Tests;

[TestClass]
public sealed class IntegrationTransactionTests
{
    [TestMethod]
    public async Task Final_validator_runs_normal_gates_without_tool_tests()
    {
        var runner = new RecordingCommandRunner();
        var validator = new CMakeIntegrationValidator(runner);

        var exit_code = await validator.ValidateAsync(
            "C:\\worktree",
            requires_tool_tests: false,
            requires_benchmark_build: true,
            TextWriter.Null,
            TextWriter.Null,
            default);

        Assert.AreEqual(0, exit_code);
        CollectionAssert.AreEqual(
            new[]
            {
                "cmake --workflow --preset debug-game-tests",
                "cmake --preset benchmark",
                "cmake --build --preset benchmark --target benchmarks",
                "cmake --workflow --preset development",
            },
            runner.Commands);
    }

    [TestMethod]
    public async Task Final_validator_runs_tool_tests_when_requested()
    {
        var runner = new RecordingCommandRunner();
        var validator = new CMakeIntegrationValidator(runner);

        var exit_code = await validator.ValidateAsync(
            "C:\\worktree",
            requires_tool_tests: true,
            requires_benchmark_build: false,
            TextWriter.Null,
            TextWriter.Null,
            default);

        Assert.AreEqual(0, exit_code);
        CollectionAssert.AreEqual(
            new[]
            {
                "cmake --workflow --preset debug-game-tests",
                "cmake --workflow --preset tool-tests",
                "cmake --workflow --preset development",
            },
            runner.Commands);
    }

    [TestMethod]
    public async Task Successful_integration_rebases_merges_and_cleans_up()
    {
        using var fixture = new TemporaryAgentGitRepository();
        var feature_worktree = CreateFeature(fixture, "dev1", "feature/one", "one.txt");
        var validation = new RecordingValidator();

        var result = await RunAsync(fixture, feature_worktree, validation);

        Assert.AreEqual(ExitCodes.Success, result.ExitCode, result.Error);
        Assert.AreEqual(1, validation.CallCount);
        Assert.AreEqual("dev1\n", fixture.RunGitAt(feature_worktree, "branch", "--show-current"));
        Assert.AreEqual(string.Empty, fixture.RunGit("branch", "--list", "feature/one"));
        Assert.AreEqual(2, fixture.RunGit("rev-list", "--parents", "-n", "1", "dev")
            .Split(' ', StringSplitOptions.RemoveEmptyEntries).Length - 1);
        Assert.AreEqual("feature one\n", fixture.RunGit("show", "dev:one.txt"));
    }

    [TestMethod]
    public async Task Completed_features_integrate_sequentially_against_the_previous_merge()
    {
        using var fixture = new TemporaryAgentGitRepository();
        var first_worktree = CreateFeature(fixture, "dev1", "feature/first", "first.txt");
        var second_worktree = CreateFeature(fixture, "dev2", "feature/second", "second.txt");
        var first_validation = new RecordingValidator();
        var second_validation = new RecordingValidator();

        var first = await RunAsync(fixture, first_worktree, first_validation);
        var first_merge = fixture.RunGit("rev-parse", "dev").Trim();
        var second = await RunAsync(fixture, second_worktree, second_validation);
        var second_feature_parent = fixture.RunGit("rev-parse", "dev^2^1").Trim();

        Assert.AreEqual(ExitCodes.Success, first.ExitCode, first.Error);
        Assert.AreEqual(ExitCodes.Success, second.ExitCode, second.Error);
        Assert.AreEqual(1, first_validation.CallCount);
        Assert.AreEqual(1, second_validation.CallCount);
        Assert.AreEqual(first_merge, second_feature_parent,
            "The second feature must be rebased once onto dev including the first merge.");
        Assert.AreEqual("feature first\n", fixture.RunGit("show", "dev:first.txt"));
        Assert.AreEqual("feature second\n", fixture.RunGit("show", "dev:second.txt"));
    }

    [TestMethod]
    public async Task Unexpected_dev_movement_aborts_without_retry_or_cleanup()
    {
        using var fixture = new TemporaryAgentGitRepository();
        var feature_worktree = CreateFeature(fixture, "dev1", "feature/movement", "feature.txt");
        var validation = new RecordingValidator(() =>
        {
            fixture.WriteFile("bypass.txt", "manual bypass\n");
            fixture.RunGit("add", "--", "bypass.txt");
            fixture.RunGit("commit", "-qm", "bypass integration protocol");
        });

        var result = await RunAsync(fixture, feature_worktree, validation);

        Assert.AreEqual(ExitCodes.StateFailure, result.ExitCode);
        Assert.AreEqual(1, validation.CallCount);
        StringAssert.Contains(result.Error, "INTEGRATION INVARIANT VIOLATION");
        StringAssert.Contains(result.Error, "Expected dev SHA:");
        StringAssert.Contains(result.Error, "Actual dev SHA:");
        Assert.AreEqual("feature/movement\n", fixture.RunGitAt(feature_worktree, "branch", "--show-current"));
        Assert.AreNotEqual(string.Empty, fixture.RunGit("branch", "--list", "feature/movement"));
        Assert.AreEqual("manual bypass\n", fixture.RunGit("show", "dev:bypass.txt"));
    }

    [TestMethod]
    public async Task Failed_validation_does_not_merge_or_delete_feature()
    {
        using var fixture = new TemporaryAgentGitRepository();
        var feature_worktree = CreateFeature(fixture, "dev1", "feature/failing", "failing.txt");
        var original_dev = fixture.RunGit("rev-parse", "dev").Trim();
        var validation = new RecordingValidator(exit_code: 42);

        var result = await RunAsync(fixture, feature_worktree, validation);

        Assert.AreEqual(42, result.ExitCode);
        Assert.AreEqual(original_dev, fixture.RunGit("rev-parse", "dev").Trim());
        Assert.AreEqual("feature/failing\n", fixture.RunGitAt(feature_worktree, "branch", "--show-current"));
        Assert.AreNotEqual(string.Empty, fixture.RunGit("branch", "--list", "feature/failing"));
    }

    [TestMethod]
    public async Task Cleanup_failure_after_merge_retains_the_merged_feature_branch()
    {
        using var fixture = new TemporaryAgentGitRepository();
        var feature_worktree = CreateFeature(fixture, "dev1", "feature/cleanup", "cleanup.txt");
        string? blocking_worktree = null;
        var validation = new RecordingValidator(() =>
        {
            blocking_worktree = fixture.CreateWorktree(
                "cleanup-blocker",
                "cleanup-blocker",
                "dev1");
            fixture.RunGitAt(blocking_worktree, "switch", "dev1");
            fixture.RunGit("branch", "-D", "cleanup-blocker");
        });

        var result = await RunAsync(fixture, feature_worktree, validation);

        Assert.AreEqual(ExitCodes.CleanupFailure, result.ExitCode);
        Assert.AreEqual("feature cleanup\n", fixture.RunGit("show", "dev:cleanup.txt"));
        Assert.AreEqual("feature/cleanup\n", fixture.RunGitAt(feature_worktree, "branch", "--show-current"));
        Assert.AreNotEqual(string.Empty, fixture.RunGit("branch", "--list", "feature/cleanup"));
        StringAssert.Contains(result.Error, "Merge succeeded, but cleanup");
        Assert.IsNotNull(blocking_worktree);
    }

    private static string CreateFeature(
        TemporaryAgentGitRepository fixture,
        string home_branch,
        string feature_branch,
        string file)
    {
        var worktree = fixture.CreateWorktree(home_branch, home_branch);
        fixture.RunGitAt(worktree, "switch", "-qc", feature_branch);
        fixture.WriteFile(file, $"feature {feature_branch.Split('/')[1]}\n", worktree);
        fixture.RunGitAt(worktree, "add", "--", file);
        fixture.RunGitAt(worktree, "commit", "-qm", $"add {feature_branch}");
        return worktree;
    }

    private static async Task<ApplicationResult> RunAsync(
        TemporaryAgentGitRepository fixture,
        string worktree,
        RecordingValidator validator)
    {
        var output = new StringWriter();
        var error = new StringWriter();
        var git = new GitClient(fixture.Trust, new ProcessRunner());
        var discovery = new RepositoryDiscovery(git);
        var transaction = new IntegrationTransaction(
            fixture.Trust,
            git,
            discovery,
            new AcceptingLeaseVerifier(),
            new AcceptingReviewer(),
            validator,
            output,
            error);
        var exit_code = await transaction.RunAsync(
            new IntegrateRequest(true, false, false), worktree, default);
        return new ApplicationResult(exit_code, output.ToString(), error.ToString());
    }

    private sealed class AcceptingLeaseVerifier : IIntegrationLeaseVerifier
    {
        public Task VerifyAsync(string resource, string worktree, CancellationToken cancellation_token)
        {
            Assert.AreEqual("integration/dev", resource);
            return Task.CompletedTask;
        }
    }

    private sealed class AcceptingReviewer : IIntegrationReviewer
    {
        public Task<bool> ConfirmAsync(string rebased_tip, CancellationToken cancellation_token) =>
            Task.FromResult(true);
    }

    private sealed class RecordingValidator(Action? action = null, int exit_code = 0) : IIntegrationValidator
    {
        public int CallCount { get; private set; }

        public Task<int> ValidateAsync(
            string worktree,
            bool requires_tool_tests,
            bool requires_benchmark_build,
            TextWriter output,
            TextWriter error,
            CancellationToken cancellation_token)
        {
            ++CallCount;
            action?.Invoke();
            return Task.FromResult(exit_code);
        }
    }

    private sealed class RecordingCommandRunner : IIntegrationCommandRunner
    {
        public List<string> Commands { get; } = [];

        public Task<int> RunAsync(
            string executable,
            IReadOnlyList<string> arguments,
            string working_directory,
            TextWriter output,
            TextWriter error,
            CancellationToken cancellation_token)
        {
            Commands.Add($"{executable} {string.Join(' ', arguments)}");
            return Task.FromResult(0);
        }
    }
}
