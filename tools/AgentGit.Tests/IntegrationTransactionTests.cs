using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace AgentGit.Tests;

[TestClass]
public sealed class IntegrationTransactionTests
{
    [TestMethod]
    public async Task Final_validator_runs_only_planned_gates_in_order()
    {
        using var fixture = new TemporaryAgentGitRepository();
        var runner = new RecordingCommandRunner();
        var validator = new IntegrationGateValidator(runner);
        var store = new IntegrationStateStore(fixture.RunGit(
            "rev-parse", "--path-format=absolute", "--git-common-dir").Trim());
        var identity = new PatchIdentity("candidate", "tree", ["tools/AgentGit.Tests/ProgramTests.cs"]);
        var plan = new IntegrationGatePlanner().Plan(identity.ChangedPaths);

        var result = await validator.ValidateAsync(
            fixture.RepositoryRoot,
            "base",
            identity,
            plan,
            store,
            TextWriter.Null,
            TextWriter.Null,
            default);
        var reused = await validator.ValidateAsync(
            fixture.RepositoryRoot,
            "base",
            identity,
            plan,
            store,
            TextWriter.Null,
            TextWriter.Null,
            default);

        Assert.AreEqual(0, result.ExitCode);
        Assert.AreEqual(0, reused.ExitCode);
        CollectionAssert.AreEqual(
            new[]
            {
                "dotnet build tools/AgentGit.Tests/AgentGit.Tests.csproj --nologo -p:IsStandaloneTool=false",
                "dotnet test tools/AgentGit.Tests/AgentGit.Tests.csproj --nologo --no-build --no-restore",
            },
            runner.Commands);

        var combined_identity = new PatchIdentity(
            "combined-candidate", "combined-tree", ["tools/AgentGit/Program.cs", "CMakeLists.txt"]);
        var combined_plan = new IntegrationGatePlanner().Plan(combined_identity.ChangedPaths);
        var combined = await validator.ValidateAsync(
            fixture.RepositoryRoot,
            "base",
            combined_identity,
            combined_plan,
            store,
            TextWriter.Null,
            TextWriter.Null,
            default);

        Assert.AreEqual(0, combined.ExitCode);
        CollectionAssert.Contains(
            runner.Commands,
            "dotnet test tools/BenchmarkTools.Tests/BenchmarkTools.Tests.csproj --nologo --no-build --no-restore");
        Assert.AreEqual(
            2,
            runner.Commands.Count(command =>
                command == "dotnet test tools/AgentGit.Tests/AgentGit.Tests.csproj --nologo --no-build --no-restore"));
    }

    [TestMethod]
    public async Task Final_validator_runs_tool_tests_when_requested()
    {
        using var fixture = new TemporaryAgentGitRepository();
        var runner = new RecordingCommandRunner();
        var validator = new IntegrationGateValidator(runner);
        var store = new IntegrationStateStore(fixture.RunGit(
            "rev-parse", "--path-format=absolute", "--git-common-dir").Trim());
        var identity = new PatchIdentity("candidate", "tree", ["tools/AgentGit.Tests/ProgramTests.cs"]);
        var plan = new IntegrationGatePlanner().Plan(identity.ChangedPaths, include_tool_tests: true);

        var result = await validator.ValidateAsync(
            fixture.RepositoryRoot,
            "base",
            identity,
            plan,
            store,
            TextWriter.Null,
            TextWriter.Null,
            default);

        Assert.AreEqual(0, result.ExitCode);
        CollectionAssert.AreEqual(
            new[]
            {
                "cmake --workflow --preset tool-tests",
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
    public async Task Moving_dev_preserves_review_for_identical_patch_and_runs_final_gate_once()
    {
        using var fixture = new TemporaryAgentGitRepository();
        var candidate_a = CreateFeature(fixture, "dev1", "feature/a", "a.txt");
        var candidate_b = CreateFeature(fixture, "dev2", "feature/b", "b.txt");
        var original_base = fixture.RunGit("rev-parse", "dev").Trim();
        var original_tip = fixture.RunGitAt(candidate_a, "rev-parse", "HEAD").Trim();
        var git = new GitClient(fixture.Trust, new ProcessRunner());
        var identity = await new PatchIdentityService(git).ComputeAsync(
            candidate_a, original_base, original_tip, default);
        var common_git = fixture.RunGit(
            "rev-parse", "--path-format=absolute", "--git-common-dir").Trim();
        new IntegrationStateStore(common_git).WriteReview(new ReviewReceipt(
            1,
            identity.Fingerprint,
            identity.Tree,
            original_base,
            original_tip,
            "feature/a",
            DateTimeOffset.UtcNow));

        var b_result = await RunAsync(fixture, candidate_b, new RecordingValidator());
        var a_validation = new RecordingValidator();
        var a_reviewer = new CountingReviewer();
        var a_result = await RunAsync(
            fixture, candidate_a, a_validation, reviewer: a_reviewer);

        Assert.AreEqual(ExitCodes.Success, b_result.ExitCode, b_result.Error);
        Assert.AreEqual(ExitCodes.Success, a_result.ExitCode, a_result.Error);
        Assert.AreEqual(0, a_reviewer.CallCount);
        Assert.AreEqual(1, a_validation.CallCount);
        StringAssert.Contains(a_result.Output, "Review receipt reused");
        Assert.AreEqual("feature b\n", fixture.RunGit("show", "dev:b.txt"));
        Assert.AreEqual("feature a\n", fixture.RunGit("show", "dev:a.txt"));
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
    public async Task Failed_validation_preserves_review_for_unchanged_effective_patch()
    {
        using var fixture = new TemporaryAgentGitRepository();
        var feature_worktree = CreateFeature(fixture, "dev1", "feature/review", "review.txt");
        var reviewer = new CountingReviewer();

        var failed = await RunAsync(
            fixture,
            feature_worktree,
            new RecordingValidator(exit_code: 42),
            reviewer: reviewer);
        var succeeded = await RunAsync(
            fixture,
            feature_worktree,
            new RecordingValidator(),
            reviewer: reviewer);

        Assert.AreEqual(42, failed.ExitCode);
        Assert.AreEqual(ExitCodes.Success, succeeded.ExitCode, succeeded.Error);
        Assert.AreEqual(1, reviewer.CallCount, "The unchanged effective patch should reuse review.");
        StringAssert.Contains(succeeded.Output, "Review receipt reused");
    }

    [TestMethod]
    public async Task Maintainer_override_skips_validation_and_is_audited_in_merge()
    {
        using var fixture = new TemporaryAgentGitRepository();
        var feature_worktree = CreateFeature(fixture, "dev1", "feature/override", "override.txt");
        var validation = new RecordingValidator(exit_code: 42);
        var reviewer = new CountingReviewer();

        var result = await RunAsync(
            fixture,
            feature_worktree,
            validation,
            new IntegrateRequest(true, false, false, true, "maintainer verified the working fix"),
            reviewer);

        Assert.AreEqual(ExitCodes.Success, result.ExitCode, result.Error);
        Assert.AreEqual(0, validation.CallCount);
        Assert.AreEqual(1, reviewer.CallCount);
        Assert.IsTrue(reviewer.SawOverride);
        StringAssert.Contains(result.Output, "Maintainer override accepted");
        var merge_message = fixture.RunGit("show", "-s", "--format=%B", "dev");
        StringAssert.Contains(merge_message, "Maintainer-Override: yes");
        StringAssert.Contains(merge_message, "Maintainer-Override-Reason: maintainer verified the working fix");
        var common_git = fixture.RunGit(
            "rev-parse", "--path-format=absolute", "--git-common-dir").Trim();
        var audit = File.ReadAllText(Path.Combine(common_git, "agent-git", "integration", "audit.jsonl"));
        StringAssert.Contains(audit, "maintainer verified the working fix");
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
        RecordingValidator validator,
        IntegrateRequest? request = null,
        IIntegrationReviewer? reviewer = null)
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
            reviewer ?? new AcceptingReviewer(),
            validator,
            output,
            error);
        var exit_code = await transaction.RunAsync(
            request ?? new IntegrateRequest(true, false, false, false, null), worktree, default);
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
        public Task<bool> ConfirmAsync(
            string patch_fingerprint,
            bool maintainer_override,
            string? override_reason,
            CancellationToken cancellation_token) =>
            Task.FromResult(true);
    }

    private sealed class CountingReviewer : IIntegrationReviewer
    {
        public int CallCount { get; private set; }

        public bool SawOverride { get; private set; }

        public Task<bool> ConfirmAsync(
            string patch_fingerprint,
            bool maintainer_override,
            string? override_reason,
            CancellationToken cancellation_token)
        {
            ++CallCount;
            SawOverride |= maintainer_override;
            return Task.FromResult(true);
        }
    }

    private sealed class RecordingValidator(Action? action = null, int exit_code = 0) : IIntegrationValidator
    {
        public int CallCount { get; private set; }

        public Task<IntegrationValidationResult> ValidateAsync(
            string worktree,
            string base_commit,
            PatchIdentity identity,
            IntegrationGatePlan plan,
            IntegrationStateStore state_store,
            TextWriter output,
            TextWriter error,
            CancellationToken cancellation_token)
        {
            ++CallCount;
            action?.Invoke();
            return Task.FromResult(exit_code == 0
                ? IntegrationValidationResult.Success
                : new IntegrationValidationResult(exit_code, "test-gate"));
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
