namespace AgentGit;

internal sealed class OperationExecutor(GitClient git)
{
    public async Task<ProcessResult> ExecuteAsync(
        EvaluatedOperation operation,
        CancellationToken cancellation_token = default)
    {
        if (!operation.Decision.Allowed)
        {
            throw new InvalidOperationException("Denied operations cannot be executed.");
        }

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
                ["switch", "--no-guess", "--no-recurse-submodules", "--", switch_request.Branch],
                cancellation_token),
            SwitchCreateRequest create => await git.RunMutationAsync(
                root,
                ["switch", "--no-guess", "--no-recurse-submodules", "--create", create.Branch],
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

    public static IReadOnlyList<string> DiagnosticArguments(EvaluatedOperation operation)
    {
        return operation.Request switch
        {
            AddRequest => ["add", "--", .. operation.ResolvedPaths!],
            AddAllRequest => ["add", "--all", "--", "."],
            CommitRequest => ["commit", "--no-verify", "--no-gpg-sign", "-m", "<message redacted>"],
            SwitchRequest switch_request => ["switch", "--no-guess", "--no-recurse-submodules", "--", switch_request.Branch],
            SwitchCreateRequest create => ["switch", "--no-guess", "--no-recurse-submodules", "--create", create.Branch],
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
