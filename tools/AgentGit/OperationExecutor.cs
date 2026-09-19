using System.Text;

namespace AgentGit;

internal sealed class OperationExecutor(GitClient git, RepositoryDiscovery discovery)
{
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
        return operation.Request switch
        {
            AddRequest => await git.RunMutationAsync(
                root,
                ["add", "--", .. operation.ResolvedPaths!],
                cancellation_token),
            AddAllRequest => await git.RunMutationAsync(root, ["add", "--all", "--", "."], cancellation_token),
            CommitRequest commit => await git.RunMutationAsync(
                root,
                ["commit", "--no-verify", "--no-gpg-sign", "-m", commit.Message],
                cancellation_token),
            SwitchRequest switch_request => await git.RunMutationAsync(
                root,
                ["switch", "--no-guess", "--no-overwrite-ignore", "--no-recurse-submodules", "--", switch_request.Branch],
                cancellation_token),
            SwitchCreateRequest create => await git.RunMutationAsync(
                root,
                ["switch", "--no-guess", "--no-overwrite-ignore", "--no-recurse-submodules", "--create", create.Branch],
                cancellation_token),
            RebaseBaseRequest => await git.RunMutationAsync(
                root,
                [
                    "rebase",
                    "--no-autostash",
                    "--no-update-refs",
                    "--no-rebase-merges",
                    $"refs/heads/{operation.Context.Policy.BaseBranch}",
                ],
                cancellation_token),
            BranchDeleteRequest delete => await git.RunMutationAsync(
                operation.BaseWorktree!.Path,
                ["branch", "--delete", "--", delete.Branch],
                cancellation_token),
            _ => throw new ArgumentOutOfRangeException(nameof(operation), operation, "Unknown mutation request."),
        };
    }

    private async Task ValidateExecutionPreconditionsAsync(
        EvaluatedOperation operation,
        CancellationToken cancellation_token)
    {
        var context = operation.Context;
        var root = context.State.WorktreeRoot;
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
            if (branch_result.ExitCode != 0)
            {
                throw new RepositoryStateException("HEAD became detached after policy evaluation.");
            }

            var branch = Encoding.UTF8.GetString(branch_result.StandardOutput).TrimEnd('\r', '\n', '\0');
            var policy_commit = await git.RequireTextAsync(
                root,
                ["rev-parse", "--verify", $"{context.Registration.PolicyRef}^{{commit}}"],
                cancellation_token);
            var base_commit = await git.RequireTextAsync(
                root,
                ["rev-parse", "--verify", $"refs/heads/{context.Policy.BaseBranch}^{{commit}}"],
                cancellation_token);

            if (!string.Equals(head, context.State.HeadCommit, StringComparison.Ordinal) ||
                !string.Equals(branch, context.State.CurrentBranch, StringComparison.Ordinal) ||
                !string.Equals(policy_commit, context.State.PolicyCommit, StringComparison.Ordinal) ||
                !string.Equals(base_commit, context.State.BaseCommit, StringComparison.Ordinal))
            {
                throw new RepositoryStateException(
                    "Critical repository refs changed after policy evaluation; no mutation was executed.");
            }

            if (operation.TargetBranch is not null)
            {
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
            RebaseBaseRequest =>
            [
                "rebase",
                "--no-autostash",
                "--no-update-refs",
                "--no-rebase-merges",
                $"refs/heads/{operation.Context.Policy.BaseBranch}",
            ],
            BranchDeleteRequest delete => ["branch", "--delete", "--", delete.Branch],
            _ => throw new ArgumentOutOfRangeException(nameof(operation), operation, "Unknown mutation request."),
        };
    }
}
