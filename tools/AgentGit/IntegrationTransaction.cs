using System.Text;

namespace AgentGit;

internal sealed class IntegrationTransaction(
    TrustContext trust,
    GitClient git,
    RepositoryDiscovery discovery,
    IIntegrationLeaseVerifier lease_verifier,
    IIntegrationReviewer reviewer,
    IIntegrationValidator validator,
    TextWriter output,
    TextWriter error)
{
    public async Task<int> RunAsync(
        IntegrateRequest request,
        string working_directory,
        CancellationToken cancellation_token)
    {
        if (!request.Authorized)
        {
            throw new RepositoryStateException("Explicit integration authorization is required.");
        }

        var context = await discovery.DiscoverAsync(trust, working_directory, cancellation_token);
        return await RunDiscoveredAsync(request, context, cancellation_token);
    }

    private async Task<int> RunDiscoveredAsync(
        IntegrateRequest request,
        RepositoryContext initial_context,
        CancellationToken cancellation_token)
    {
        var state = initial_context.State;
        var base_branch = initial_context.Policy.BaseBranch;
        var resource = $"integration/{base_branch}";
        await lease_verifier.VerifyAsync(resource, state.WorktreeRoot, cancellation_token);
        ValidateFeatureState(initial_context);

        var base_worktrees = state.Worktrees.Where(worktree =>
            string.Equals(worktree.Branch, base_branch, StringComparison.Ordinal)).ToArray();
        if (base_worktrees.Length != 1)
        {
            throw new RepositoryStateException(
                $"Integration requires exactly one worktree on '{base_branch}'; found {base_worktrees.Length}.");
        }

        var base_worktree = base_worktrees[0];
        var base_context = await discovery.DiscoverAsync(
            trust,
            base_worktree.Path,
            cancellation_token);
        ValidateBaseState(base_context, base_branch);
        var integration_base_sha = base_context.State.HeadCommit;
        var original_tip = state.HeadCommit;
        if (string.Equals(original_tip, integration_base_sha, StringComparison.Ordinal))
        {
            throw new RepositoryStateException("The feature branch has no commits to integrate.");
        }
        var original_base = await git.RequireTextAsync(
            state.WorktreeRoot,
            ["merge-base", original_tip, integration_base_sha],
            cancellation_token);

        output.WriteLine($"Integration resource: {resource}");
        output.WriteLine($"Integration base SHA: {integration_base_sha}");
        output.WriteLine($"Feature branch: {state.CurrentBranch}");
        output.WriteLine($"Feature tip before rebase: {original_tip}");

        await RebaseAsync(initial_context, integration_base_sha, cancellation_token);
        var rebased_tip = await git.RequireTextAsync(
            state.WorktreeRoot,
            ["rev-parse", "--verify", "HEAD^{commit}"],
            cancellation_token);
        var ancestry = await git.RunAsync(
            state.WorktreeRoot,
            ["merge-base", "--is-ancestor", integration_base_sha, rebased_tip],
            cancellation_token: cancellation_token);
        if (ancestry.ExitCode != 0)
        {
            throw new RepositoryStateException(
                "The rebased feature is not descended from the recorded integration base.");
        }
        await ShowReviewMaterialAsync(
            state.WorktreeRoot,
            original_base,
            original_tip,
            integration_base_sha,
            rebased_tip,
            cancellation_token);
        if (!await reviewer.ConfirmAsync(rebased_tip, cancellation_token))
        {
            error.WriteLine("Integration review was not confirmed; the transaction has been aborted.");
            return ExitCodes.StateFailure;
        }

        var changed_paths = await git.RequireTextAsync(
            state.WorktreeRoot,
            ["diff", "--name-only", $"{integration_base_sha}..{rebased_tip}"],
            cancellation_token);
        var requires_benchmark_build = changed_paths.Split('\n', StringSplitOptions.RemoveEmptyEntries)
            .Any(path => path.Contains("benchmark", StringComparison.OrdinalIgnoreCase));
        var validation_exit = await validator.ValidateAsync(
            state.WorktreeRoot,
            request.ToolTests,
            requires_benchmark_build,
            output,
            error,
            cancellation_token);
        if (validation_exit != 0)
        {
            error.WriteLine(
                $"Final integration validation failed with exit code {validation_exit}; nothing was merged. " +
                "Release the reservation, fix with focused validation, and requeue when ready.");
            return validation_exit;
        }

        await lease_verifier.VerifyAsync(resource, state.WorktreeRoot, cancellation_token);

        var merge_exit = await MergeAsync(
            initial_context,
            base_worktree.Path,
            integration_base_sha,
            rebased_tip,
            cancellation_token);
        if (merge_exit != ExitCodes.Success)
        {
            return merge_exit;
        }

        return await CleanupAsync(
            request,
            initial_context,
            base_worktree.Path,
            rebased_tip,
            cancellation_token);
    }

    private async Task RebaseAsync(
        RepositoryContext context,
        string integration_base_sha,
        CancellationToken cancellation_token)
    {
        await using var repository_lock = await RepositoryLock.AcquireAsync(
            context.State.CommonGitDirectory,
            cancellation_token);
        var result = await git.RunMutationAsync(
            context.State.WorktreeRoot,
            ["rebase", "--no-autostash", "--no-update-refs", "--no-rebase-merges", integration_base_sha],
            cancellation_token);
        if (result.ExitCode == 0)
        {
            WriteProcessResult(result);
            return;
        }

        WriteProcessResult(result);
        var abort = await git.RunMutationAsync(
            context.State.WorktreeRoot,
            ["rebase", "--abort"],
            cancellation_token);
        if (abort.ExitCode != 0)
        {
            WriteProcessResult(abort);
            throw new RepositoryStateException(
                "Rebase failed and Git could not abort it cleanly; the feature branch was not merged.");
        }

        throw new GitCommandException(
            "Final rebase failed and was aborted; resolve the conflict outside the integration queue, then requeue.",
            result.ExitCode,
            result.StandardError);
    }

    private async Task ShowReviewMaterialAsync(
        string root,
        string original_base,
        string original_tip,
        string integration_base_sha,
        string rebased_tip,
        CancellationToken cancellation_token)
    {
        output.WriteLine();
        output.WriteLine("Rebased commit range comparison:");
        var range_diff = await git.RunAsync(
            root,
            ["range-diff", $"{original_base}..{original_tip}", $"{integration_base_sha}..{rebased_tip}"],
            cancellation_token: cancellation_token);
        WriteProcessResult(range_diff);
        GitClient.EnsureSuccess(range_diff, ["range-diff"]);

        output.WriteLine();
        output.WriteLine("Rebased feature diff summary:");
        var summary = await git.RunAsync(
            root,
            ["diff", "--stat", $"{integration_base_sha}..{rebased_tip}"],
            cancellation_token: cancellation_token);
        WriteProcessResult(summary);
        GitClient.EnsureSuccess(summary, ["diff"]);
        var check = await git.RunAsync(
            root,
            ["diff", "--check", $"{integration_base_sha}..{rebased_tip}"],
            cancellation_token: cancellation_token);
        WriteProcessResult(check);
        GitClient.EnsureSuccess(check, ["diff"]);
        output.WriteLine($"Rebased feature tip: {rebased_tip}");
    }

    private async Task<int> MergeAsync(
        RepositoryContext initial_context,
        string base_worktree,
        string integration_base_sha,
        string rebased_tip,
        CancellationToken cancellation_token)
    {
        await using var repository_lock = await RepositoryLock.AcquireAsync(
            initial_context.State.CommonGitDirectory,
            cancellation_token);
        var feature_context = await discovery.DiscoverAsync(
            trust,
            initial_context.State.WorktreeRoot,
            cancellation_token);
        ValidateFeatureState(feature_context);
        if (!string.Equals(feature_context.State.HeadCommit, rebased_tip, StringComparison.Ordinal))
        {
            throw new RepositoryStateException(
                "Feature HEAD changed after final validation; nothing was merged.");
        }

        var base_context = await discovery.DiscoverAsync(
            trust,
            base_worktree,
            cancellation_token);
        ValidateBaseState(base_context, initial_context.Policy.BaseBranch);
        var actual_base_sha = base_context.State.HeadCommit;
        if (!string.Equals(actual_base_sha, integration_base_sha, StringComparison.Ordinal))
        {
            error.WriteLine("INTEGRATION INVARIANT VIOLATION: dev moved while its reservation was held.");
            error.WriteLine($"Expected dev SHA: {integration_base_sha}");
            error.WriteLine($"Actual dev SHA:   {actual_base_sha}");
            error.WriteLine("The transaction was aborted without retrying or rebasing again.");
            return ExitCodes.StateFailure;
        }

        var feature_tree = await git.RequireTextAsync(
            feature_context.State.WorktreeRoot,
            ["rev-parse", $"{rebased_tip}^{{tree}}"],
            cancellation_token);
        var commit_result = await git.RunMutationAsync(
            base_worktree,
            [
                "commit-tree",
                feature_tree,
                "-p", integration_base_sha,
                "-p", rebased_tip,
                "-m", $"Merge branch '{feature_context.State.CurrentBranch}' into {initial_context.Policy.BaseBranch}",
            ],
            cancellation_token);
        WriteProcessResult(commit_result);
        GitClient.EnsureSuccess(commit_result, ["commit-tree"]);
        var merge_commit = Encoding.UTF8.GetString(commit_result.StandardOutput).Trim();

        var update = await git.RunMutationAsync(
            base_worktree,
            [
                "update-ref",
                $"refs/heads/{initial_context.Policy.BaseBranch}",
                merge_commit,
                integration_base_sha,
            ],
            cancellation_token);
        WriteProcessResult(update);
        if (update.ExitCode != 0)
        {
            var current = await git.RequireTextAsync(
                base_worktree,
                ["rev-parse", $"refs/heads/{initial_context.Policy.BaseBranch}^{{commit}}"],
                cancellation_token);
            error.WriteLine("INTEGRATION INVARIANT VIOLATION: the atomic dev update was rejected.");
            error.WriteLine($"Expected dev SHA: {integration_base_sha}");
            error.WriteLine($"Actual dev SHA:   {current}");
            error.WriteLine("The transaction was aborted without retrying or rebasing again.");
            return ExitCodes.StateFailure;
        }

        var reset = await git.RunMutationAsync(
            base_worktree,
            ["reset", "--hard", merge_commit],
            cancellation_token);
        WriteProcessResult(reset);
        if (reset.ExitCode != 0)
        {
            error.WriteLine(
                $"dev now contains merge {merge_commit}, but worktree '{base_worktree}' could not be refreshed. " +
                "The feature branch is retained for recovery.");
            return ExitCodes.CleanupFailure;
        }

        output.WriteLine($"Merged validated feature tip {rebased_tip} into {initial_context.Policy.BaseBranch}.");
        output.WriteLine($"Merge commit: {merge_commit}");
        return ExitCodes.Success;
    }

    private async Task<int> CleanupAsync(
        IntegrateRequest request,
        RepositoryContext initial_context,
        string base_worktree,
        string rebased_tip,
        CancellationToken cancellation_token)
    {
        await using var repository_lock = await RepositoryLock.AcquireAsync(
            initial_context.State.CommonGitDirectory,
            cancellation_token);
        var feature_branch = initial_context.State.CurrentBranch!;
        var home_branch = initial_context.State.HomeBranch;
        if (home_branch is null)
        {
            output.WriteLine("No persistent home branch exists for this worktree; the merged feature branch was retained.");
            return ExitCodes.Success;
        }

        var switch_result = await git.RunMutationAsync(
            initial_context.State.WorktreeRoot,
            ["switch", "--no-guess", "--no-overwrite-ignore", "--no-recurse-submodules", "--", home_branch],
            cancellation_token);
        WriteProcessResult(switch_result);
        if (switch_result.ExitCode != 0)
        {
            error.WriteLine(
                $"Merge succeeded, but cleanup could not return the worktree to '{home_branch}'. " +
                $"Feature branch '{feature_branch}' was retained.");
            return ExitCodes.CleanupFailure;
        }

        if (request.KeepBranch)
        {
            output.WriteLine($"Returned worktree to '{home_branch}'; retained '{feature_branch}' as requested.");
            return ExitCodes.Success;
        }

        var current_tip = await git.RequireTextAsync(
            base_worktree,
            ["rev-parse", $"refs/heads/{feature_branch}^{{commit}}"],
            cancellation_token);
        if (!string.Equals(current_tip, rebased_tip, StringComparison.Ordinal))
        {
            error.WriteLine(
                $"Merge succeeded, but feature branch '{feature_branch}' moved during cleanup and was retained.");
            return ExitCodes.CleanupFailure;
        }

        var delete = await git.RunMutationAsync(
            base_worktree,
            ["branch", "--delete", "--", feature_branch],
            cancellation_token);
        WriteProcessResult(delete);
        if (delete.ExitCode != 0)
        {
            error.WriteLine(
                $"Merge succeeded, but safe deletion of '{feature_branch}' failed; the branch was retained.");
            return ExitCodes.CleanupFailure;
        }

        output.WriteLine($"Returned worktree to '{home_branch}' and safely deleted '{feature_branch}'.");
        return ExitCodes.Success;
    }

    private static void ValidateFeatureState(RepositoryContext context)
    {
        var state = context.State;
        if (state.CurrentClassification != BranchClassification.Feature || state.CurrentBranch is null)
        {
            throw new RepositoryStateException("Integration must start from a checked-out feature branch.");
        }
        if (!state.Status.IsClean)
        {
            throw new RepositoryStateException(
                "Integration requires a clean feature index and working tree, including untracked files.");
        }
        if (state.OperationState != RepositoryOperationState.None)
        {
            throw new RepositoryStateException(
                $"A {state.OperationState} operation is already in progress in the feature worktree.");
        }
    }

    private static void ValidateBaseState(RepositoryContext context, string base_branch)
    {
        var state = context.State;
        if (!string.Equals(state.CurrentBranch, base_branch, StringComparison.Ordinal) ||
            state.CurrentClassification != BranchClassification.Protected)
        {
            throw new RepositoryStateException(
                $"The integration worktree is no longer on protected branch '{base_branch}'.");
        }
        if (!state.Status.IsClean || state.OperationState != RepositoryOperationState.None)
        {
            throw new RepositoryStateException(
                $"The '{base_branch}' worktree must be clean and have no Git operation in progress.");
        }
        if (!string.Equals(state.HeadCommit, state.BaseCommit, StringComparison.Ordinal))
        {
            throw new RepositoryStateException(
                $"The '{base_branch}' worktree HEAD does not match refs/heads/{base_branch}.");
        }
    }

    private void WriteProcessResult(ProcessResult result)
    {
        if (result.StandardOutput.Length > 0)
        {
            output.Write(Encoding.UTF8.GetString(result.StandardOutput));
        }
        if (!string.IsNullOrEmpty(result.StandardError))
        {
            error.Write(result.StandardError);
        }
    }
}
