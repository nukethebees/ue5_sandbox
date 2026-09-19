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
        output.WriteLine("Integration phase: preflight");
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
        var identity_service = new PatchIdentityService(git);
        var original_identity = await identity_service.ComputeAsync(
            state.WorktreeRoot,
            original_base,
            original_tip,
            cancellation_token);

        output.WriteLine($"Integration resource: {resource}");
        output.WriteLine($"Integration base SHA: {integration_base_sha}");
        output.WriteLine($"Feature branch: {state.CurrentBranch}");
        output.WriteLine($"Feature tip before rebase: {original_tip}");

        output.WriteLine("Integration phase: final-rebase");
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
        var final_identity = await identity_service.ComputeAsync(
            state.WorktreeRoot,
            integration_base_sha,
            rebased_tip,
            cancellation_token);
        var state_store = new IntegrationStateStore(state.CommonGitDirectory);
        var gate_planner = await IntegrationGatePlanner.LoadAsync(
            git,
            state.WorktreeRoot,
            integration_base_sha,
            cancellation_token);
        var plan = gate_planner.Plan(final_identity.ChangedPaths, request.ToolTests);
        output.WriteLine("Integration phase: review");
        await ShowReviewMaterialAsync(
            state.WorktreeRoot,
            original_base,
            original_tip,
            integration_base_sha,
            rebased_tip,
            final_identity,
            cancellation_token);

        if (!string.Equals(original_identity.Fingerprint, final_identity.Fingerprint, StringComparison.Ordinal))
        {
            output.WriteLine("Review invalidated: effective patch changed during the final rebase.");
            output.WriteLine($"Previous patch: {original_identity.Fingerprint}");
            output.WriteLine($"Final patch:    {final_identity.Fingerprint}");
        }

        WriteGatePlan(plan);
        var review = state_store.ReadReview(final_identity.Fingerprint);
        var requires_confirmation = request.MaintainerOverride || review is null;
        if (requires_confirmation && !await reviewer.ConfirmAsync(
                final_identity.Fingerprint,
                request.MaintainerOverride,
                request.OverrideReason,
                cancellation_token))
        {
            error.WriteLine(request.MaintainerOverride
                ? "Integration blocked: maintainer override confirmation did not match the final candidate."
                : "Integration blocked: final effective patch review was not confirmed.");
            return ExitCodes.StateFailure;
        }

        if (review is not null && !request.MaintainerOverride)
        {
            output.WriteLine(
                $"Review receipt reused for effective patch {final_identity.Fingerprint} " +
                $"from {review.ReviewedAt:u}.");
        }
        else if (!request.MaintainerOverride)
        {
            state_store.WriteReview(new ReviewReceipt(
                1,
                final_identity.Fingerprint,
                final_identity.Tree,
                integration_base_sha,
                rebased_tip,
                state.CurrentBranch!,
                DateTimeOffset.UtcNow));
        }

        if (request.MaintainerOverride)
        {
            output.WriteLine("Maintainer override accepted. Skipped policy gates:");
            if (review is null)
            {
                output.WriteLine("  review");
            }
            foreach (var gate in plan.Gates)
            {
                output.WriteLine($"  {IntegrationGatePlanner.Name(gate)}");
            }
            AppendAuditSafely(state_store, new
            {
                version = 1,
                eventName = "maintainer-override-authorized",
                timestamp = DateTimeOffset.UtcNow,
                candidate = final_identity.Fingerprint,
                candidateTree = final_identity.Tree,
                integrationBase = integration_base_sha,
                skippedGates = SkippedGates(plan, review is null),
                overrideReason = request.OverrideReason,
                worktree = state.WorktreeRoot,
                directory = Environment.CurrentDirectory,
                lease = Environment.GetEnvironmentVariable("NUKETHEBEES_JOBSERVER_LEASE"),
                task = Environment.GetEnvironmentVariable("NUKETHEBEES_JOBSERVER_TASK"),
            });
        }
        else
        {
            output.WriteLine("Integration phase: final-validation");
            var validation = await validator.ValidateAsync(
                state.WorktreeRoot,
                integration_base_sha,
                final_identity,
                plan,
                state_store,
                output,
                error,
                cancellation_token);
            if (validation.ExitCode != 0)
            {
                error.WriteLine(
                    $"Required action: fix {validation.FailedGate}, use focused validation, and requeue. " +
                    "The review receipt remains valid while the effective patch is unchanged.");
                AppendAuditSafely(state_store, new
                {
                    version = 1,
                    eventName = "validation-failed",
                    timestamp = DateTimeOffset.UtcNow,
                    candidate = final_identity.Fingerprint,
                    integrationBase = integration_base_sha,
                    failedGate = validation.FailedGate,
                    worktree = state.WorktreeRoot,
                });
                return validation.ExitCode;
            }
        }

        await lease_verifier.VerifyAsync(resource, state.WorktreeRoot, cancellation_token);

        output.WriteLine("Integration phase: atomic-promotion");
        var merge_exit = await MergeAsync(
            initial_context,
            base_worktree.Path,
            integration_base_sha,
            rebased_tip,
            final_identity,
            plan,
            request,
            request.MaintainerOverride && review is null,
            cancellation_token);
        if (merge_exit != ExitCodes.Success)
        {
            return merge_exit;
        }

        output.WriteLine("Integration phase: cleanup");
        return await CleanupAsync(
            request,
            initial_context,
            base_worktree.Path,
            rebased_tip,
            cancellation_token);
    }

    private void WriteGatePlan(IntegrationGatePlan plan)
    {
        output.WriteLine();
        output.WriteLine($"Integration components: {(plan.Components.Count == 0 ? "none" : string.Join(", ", plan.Components))}");
        if (plan.Gates.Count == 0)
        {
            output.WriteLine("Integration gates: cheap repository sanity checks only.");
            return;
        }

        output.WriteLine("Integration gates:");
        foreach (var gate in plan.Gates)
        {
            output.WriteLine($"  {IntegrationGatePlanner.Name(gate)}: {plan.Reasons[gate]}");
        }
        output.WriteLine(plan.RequiresUnreal
            ? "Unreal validation is required by the changed dependency surface."
            : "No Unreal build or test is required for this candidate.");
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
        PatchIdentity identity,
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
        output.WriteLine($"Effective patch fingerprint: {identity.Fingerprint}");
        output.WriteLine($"Candidate tree: {identity.Tree}");
    }

    private async Task<int> MergeAsync(
        RepositoryContext initial_context,
        string base_worktree,
        string integration_base_sha,
        string rebased_tip,
        PatchIdentity identity,
        IntegrationGatePlan plan,
        IntegrateRequest request,
        bool review_was_skipped,
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
                "-m", MergeMessage(feature_context.State.CurrentBranch!, initial_context.Policy.BaseBranch,
                    identity, plan, request, review_was_skipped),
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

        output.WriteLine($"Merged integration candidate {rebased_tip} into {initial_context.Policy.BaseBranch}.");
        output.WriteLine($"Merge commit: {merge_commit}");
        var state_store = new IntegrationStateStore(initial_context.State.CommonGitDirectory);
        AppendAuditSafely(state_store, new
        {
            version = 1,
            eventName = "integration-merged",
            timestamp = DateTimeOffset.UtcNow,
            candidate = identity.Fingerprint,
            candidateTree = identity.Tree,
            integrationBase = integration_base_sha,
            featureTip = rebased_tip,
            mergeCommit = merge_commit,
            gates = plan.Gates.Select(IntegrationGatePlanner.Name).ToArray(),
            skippedGates = request.MaintainerOverride
                ? SkippedGates(plan, review_was_skipped)
                : [],
            maintainerOverride = request.MaintainerOverride,
            overrideReason = request.OverrideReason,
            worktree = initial_context.State.WorktreeRoot,
            directory = Environment.CurrentDirectory,
            lease = Environment.GetEnvironmentVariable("NUKETHEBEES_JOBSERVER_LEASE"),
            task = Environment.GetEnvironmentVariable("NUKETHEBEES_JOBSERVER_TASK"),
        });
        return ExitCodes.Success;
    }

    private void AppendAuditSafely(IntegrationStateStore state_store, object entry)
    {
        try
        {
            state_store.AppendAudit(entry);
        }
        catch (Exception exception) when (exception is IOException or UnauthorizedAccessException)
        {
            error.WriteLine(
                $"Integration audit log warning: {exception.Message} " +
                "Successful overrides remain recorded in the merge commit trailers.");
        }
    }

    private static string[] SkippedGates(IntegrationGatePlan plan, bool review_was_skipped)
    {
        var gates = plan.Gates.Select(IntegrationGatePlanner.Name);
        return (review_was_skipped ? gates.Prepend("review") : gates).ToArray();
    }

    private static string MergeMessage(
        string feature_branch,
        string base_branch,
        PatchIdentity identity,
        IntegrationGatePlan plan,
        IntegrateRequest request,
        bool review_was_skipped)
    {
        var message = new StringBuilder()
            .Append("Merge branch '").Append(feature_branch).Append("' into ").Append(base_branch)
            .Append("\n\nIntegration-Candidate: ").Append(identity.Fingerprint)
            .Append("\nIntegration-Tree: ").Append(identity.Tree)
            .Append("\nIntegration-Gates: ")
            .Append(plan.Gates.Count == 0 ? "sanity-only" : string.Join(',', plan.Gates.Select(IntegrationGatePlanner.Name)));
        if (request.MaintainerOverride)
        {
            message.Append("\nMaintainer-Override: yes")
                .Append("\nIntegration-Skipped-Gates: ")
                .Append(string.Join(',', SkippedGates(plan, review_was_skipped)))
                .Append("\nMaintainer-Override-Reason: ")
                .Append(request.OverrideReason!.Replace('\r', ' ').Replace('\n', ' '));
        }

        return message.ToString();
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
