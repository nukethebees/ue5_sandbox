using System.Text;

namespace AgentGit;

internal sealed class OperationExecutor(GitClient git, RepositoryDiscovery discovery)
{
    private static readonly IReadOnlyList<string> rebase_start_arguments_prefix =
    [
        "-c", "rebase.instructionFormat=# %s",
        "-c", "rebase.abbreviateCommands=false",
        "-c", "rebase.rescheduleFailedExec=false",
        "rebase",
        "--merge",
        "--no-verify",
        "--no-autostash",
        "--no-update-refs",
        "--no-rebase-merges",
        "--strategy=ort",
    ];

    private static readonly IReadOnlyList<string> rebase_continue_arguments =
    [
        "-c", "rebase.rescheduleFailedExec=false",
        "rebase",
        "--continue",
    ];

    public async Task<ProcessResult> ExecuteAsync(
        EvaluatedOperation operation,
        CancellationToken cancellation_token = default)
    {
        if (!operation.Decision.Allowed)
        {
            throw new InvalidOperationException("Denied operations cannot be executed.");
        }

        await ValidateExecutionPreconditionsAsync(operation, cancellation_token);
        var root = operation.Context.State.WorktreeRoot;
        switch (operation.Request)
        {
            case AddRequest:
                return await git.RunMutationAsync(
                    root,
                    ["add", "--", .. operation.ResolvedPaths!],
                    cancellation_token);
            case AddAllRequest:
                return await git.RunMutationAsync(root, ["add", "--all", "--", "."], cancellation_token);
            case CommitRequest commit:
                return await git.RunMutationAsync(
                    root,
                    ["commit", "--no-verify", "--no-gpg-sign", "-m", commit.Message],
                    cancellation_token);
            case SwitchRequest switch_request:
                return await git.RunMutationAsync(
                    root,
                    ["switch", "--no-guess", "--no-overwrite-ignore", "--no-recurse-submodules", "--", switch_request.Branch],
                    cancellation_token);
            case SwitchCreateRequest create:
                return await git.RunMutationAsync(
                    root,
                    ["switch", "--no-guess", "--no-overwrite-ignore", "--no-recurse-submodules", "--create", create.Branch],
                    cancellation_token);
            case RebaseBaseRequest:
                discovery.RebaseRecovery.Prepare(operation.Context);
                return await git.RunMutationAsync(
                    root,
                    [.. rebase_start_arguments_prefix, operation.Context.State.BaseCommit],
                    cancellation_token);
            case RebaseContinueRequest:
                return await git.RunMutationWithNoOpEditorAsync(
                    root,
                    rebase_continue_arguments,
                    cancellation_token);
            case RebaseAbortRequest:
                return await git.RunMutationAsync(root, ["rebase", "--abort"], cancellation_token);
            case BranchDeleteRequest delete:
                return await git.RunMutationAsync(
                    operation.BaseWorktree!.Path,
                    ["branch", "--delete", "--", delete.Branch],
                    cancellation_token);
            default:
                throw new ArgumentOutOfRangeException(nameof(operation), operation, "Unknown mutation request.");
        }
    }

    public async Task FinalizeAsync(
        EvaluatedOperation operation,
        ProcessResult result,
        RepositoryContext post_context,
        CancellationToken cancellation_token = default)
    {
        switch (operation.Request)
        {
            case RebaseBaseRequest:
                await FinalizeStartAsync(operation.Context, result, post_context, cancellation_token);
                break;
            case RebaseContinueRequest:
                await FinalizeContinueAsync(operation.Context, result, post_context, cancellation_token);
                break;
            case RebaseAbortRequest:
                await FinalizeAbortAsync(operation.Context, result, post_context);
                break;
        }
    }

    private async Task FinalizeStartAsync(
        RepositoryContext before,
        ProcessResult result,
        RepositoryContext after,
        CancellationToken cancellation_token)
    {
        if (result.ExitCode == 0)
        {
            await ValidateCompletedRebaseAsync(before, after, cancellation_token);
            discovery.RebaseRecovery.RemoveMarker(after.State.GitDirectory);
            return;
        }

        if (after.State.OperationState == RepositoryOperationState.Rebase)
        {
            await discovery.RebaseRecovery.PromoteAsync(before, after, cancellation_token);
            return;
        }

        discovery.RebaseRecovery.RemoveMarker(after.State.GitDirectory);
    }

    private async Task FinalizeContinueAsync(
        RepositoryContext before,
        ProcessResult result,
        RepositoryContext after,
        CancellationToken cancellation_token)
    {
        if (result.ExitCode == 0)
        {
            await ValidateCompletedRebaseAsync(before, after, cancellation_token);
            discovery.RebaseRecovery.RemoveMarker(after.State.GitDirectory);
            return;
        }

        if (!after.State.RebaseRecovery.IsOwned)
        {
            throw new RepositoryStateException(
                "Git rebase continuation ended in an unexpected state; recovery metadata was retained.");
        }
    }

    private Task FinalizeAbortAsync(
        RepositoryContext before,
        ProcessResult result,
        RepositoryContext after)
    {
        if (result.ExitCode != 0)
        {
            if (!after.State.RebaseRecovery.IsOwned)
            {
                throw new RepositoryStateException(
                    "Git rebase abort failed and recovery state no longer matches AgentGit provenance.");
            }

            return Task.CompletedTask;
        }

        var marker = before.State.RebaseRecovery.Marker!;
        if (after.State.OperationState != RepositoryOperationState.None ||
            !string.Equals(after.State.CurrentBranch, marker.OriginalBranch, StringComparison.Ordinal) ||
            !string.Equals(after.State.HeadCommit, marker.OriginalHead, StringComparison.OrdinalIgnoreCase))
        {
            throw new RepositoryStateException(
                "Git reported a successful rebase abort without restoring the original feature branch and HEAD.");
        }

        discovery.RebaseRecovery.RemoveMarker(after.State.GitDirectory);
        return Task.CompletedTask;
    }

    private async Task ValidateCompletedRebaseAsync(
        RepositoryContext before,
        RepositoryContext after,
        CancellationToken cancellation_token)
    {
        var marker = before.State.RebaseRecovery.Marker ?? new RebaseRecoveryMarker(
            1,
            "prepared",
            string.Empty,
            before.Registration.RepositoryId,
            before.State.CurrentBranch!,
            before.State.HeadCommit,
            before.Policy.BaseBranch,
            before.State.BaseCommit,
            before.State.PolicyCommit,
            null);
        if (after.State.OperationState != RepositoryOperationState.None ||
            !string.Equals(after.State.CurrentBranch, marker.OriginalBranch, StringComparison.Ordinal) ||
            after.State.CurrentClassification != BranchClassification.Feature ||
            !after.Policy.Operations.TryGetValue(AgentGitOperation.RebaseBase, out var rebase_policy) ||
            !rebase_policy.AllowedCurrentGroups.Contains(BranchClassification.Feature))
        {
            throw new RepositoryStateException(
                "Git reported a successful rebase without restoring the expected feature branch and repository state.");
        }

        var based_on_recorded_commit = await git.RunAsync(
            after.State.WorktreeRoot,
            ["merge-base", "--is-ancestor", marker.BaseCommit, after.State.HeadCommit],
            cancellation_token: cancellation_token);
        if (based_on_recorded_commit.ExitCode != 0)
        {
            throw new RepositoryStateException(
                "Completed rebase is not based on the exact base commit AgentGit authorized.");
        }
    }

    private async Task ValidateExecutionPreconditionsAsync(
        EvaluatedOperation operation,
        CancellationToken cancellation_token)
    {
        var context = operation.Context;
        var state = context.State;
        var root = state.WorktreeRoot;
        try
        {
            await discovery.RevalidateMutationSnapshotAsync(context, cancellation_token);
            var head = await git.RequireTextAsync(
                root,
                ["rev-parse", "--verify", "HEAD^{commit}"],
                cancellation_token);
            var branch_result = await git.RunAsync(
                root,
                ["symbolic-ref", "--quiet", "--short", "--no-recurse", "HEAD"],
                cancellation_token: cancellation_token);
            var recovery_operation = operation.Request is RebaseContinueRequest or RebaseAbortRequest ||
                (operation.Request is AddRequest or AddAllRequest &&
                 state.OperationState == RepositoryOperationState.Rebase);
            string? branch;
            if (recovery_operation)
            {
                if (branch_result.ExitCode != 1)
                {
                    throw new RepositoryStateException(
                        "HEAD is no longer detached as required by AgentGit rebase recovery.");
                }

                branch = null;
                await discovery.RevalidateDirectBranchAsync(
                    root,
                    state.RebaseRecovery.Marker!.OriginalBranch,
                    cancellation_token);
            }
            else
            {
                if (branch_result.ExitCode != 0)
                {
                    throw new RepositoryStateException("HEAD became detached after policy evaluation.");
                }

                branch = Encoding.UTF8.GetString(branch_result.StandardOutput).TrimEnd('\r', '\n', '\0');
            }

            var policy_commit = await git.RequireTextAsync(
                root,
                ["rev-parse", "--verify", $"{context.Registration.PolicyRef}^{{commit}}"],
                cancellation_token);
            var base_commit = await git.RequireTextAsync(
                root,
                ["rev-parse", "--verify", $"refs/heads/{context.Policy.BaseBranch}^{{commit}}"],
                cancellation_token);

            if (!string.Equals(head, state.HeadCommit, StringComparison.Ordinal) ||
                !string.Equals(branch, state.CurrentBranch, StringComparison.Ordinal) ||
                !string.Equals(policy_commit, state.PolicyCommit, StringComparison.Ordinal) ||
                !string.Equals(base_commit, state.BaseCommit, StringComparison.Ordinal))
            {
                throw new RepositoryStateException(
                    "Critical repository refs changed after policy evaluation; no mutation was executed.");
            }

            if (operation.TargetBranch is not null)
            {
                await discovery.RevalidateDirectBranchAsync(
                    root,
                    operation.TargetBranch.Name,
                    cancellation_token);
                var target_commit = await git.RequireTextAsync(
                    root,
                    ["rev-parse", "--verify", $"refs/heads/{operation.TargetBranch.Name}^{{commit}}"],
                    cancellation_token);
                if (!string.Equals(target_commit, operation.TargetBranch.Commit, StringComparison.Ordinal))
                {
                    throw new RepositoryStateException(
                        $"Target branch '{operation.TargetBranch.Name}' changed after policy evaluation; " +
                        "no mutation was executed.");
                }
            }

            if (operation.BaseWorktree is not null)
            {
                await discovery.RevalidateWorktreeAsync(
                    context,
                    operation.BaseWorktree,
                    context.Policy.BaseBranch,
                    state.BaseCommit,
                    cancellation_token);
            }
        }
        catch (GitCommandException exception)
        {
            throw new RepositoryStateException(
                "Unable to revalidate critical repository refs immediately before mutation.", exception);
        }
    }

    public static IReadOnlyList<string> DiagnosticArguments(EvaluatedOperation operation)
    {
        return operation.Request switch
        {
            AddRequest => ["add", "--", .. operation.ResolvedPaths!],
            AddAllRequest => ["add", "--all", "--", "."],
            CommitRequest => ["commit", "--no-verify", "--no-gpg-sign", "-m", "<message redacted>"],
            SwitchRequest switch_request =>
                ["switch", "--no-guess", "--no-overwrite-ignore", "--no-recurse-submodules", "--", switch_request.Branch],
            SwitchCreateRequest create =>
                ["switch", "--no-guess", "--no-overwrite-ignore", "--no-recurse-submodules", "--create", create.Branch],
            RebaseBaseRequest => [.. rebase_start_arguments_prefix, operation.Context.State.BaseCommit],
            RebaseContinueRequest => rebase_continue_arguments,
            RebaseAbortRequest => ["rebase", "--abort"],
            BranchDeleteRequest delete => ["branch", "--delete", "--", delete.Branch],
            _ => throw new ArgumentOutOfRangeException(nameof(operation), operation, "Unknown mutation request."),
        };
    }
}
