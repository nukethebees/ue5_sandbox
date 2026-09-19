namespace AgentGit;

internal sealed class PolicyEvaluator(RepositoryDiscovery discovery)
{
    public async Task<EvaluatedOperation> EvaluateAsync(
        MutationRequest request,
        RepositoryContext context,
        string invocation_directory,
        CancellationToken cancellation_token = default)
    {
        if (!context.Policy.Operations.TryGetValue(request.Operation, out var operation_policy))
        {
            return Denied(request, context,
                $"Operation '{CommandLine.OperationName(request.Operation)}' is not enabled by repository policy.");
        }

        var state = context.State;
        if (state.CurrentBranch is null || state.CurrentClassification is null)
        {
            return Denied(request, context, "Mutating operations are forbidden in detached HEAD state.");
        }

        if (state.OperationState != RepositoryOperationState.None)
        {
            return Denied(request, context,
                $"A {state.OperationState} operation is already in progress; agent-git will not begin another mutation.");
        }

        if (!operation_policy.AllowedCurrentGroups.Contains(state.CurrentClassification.Value))
        {
            return Denied(request, context,
                $"Operation '{CommandLine.OperationName(request.Operation)}' is forbidden on " +
                $"{state.CurrentClassification.Value.ToString().ToLowerInvariant()} branch '{state.CurrentBranch}'.");
        }

        if (operation_policy.RequireClean && !state.Status.IsClean)
        {
            return Denied(request, context, "Operation requires a clean index and working tree, including untracked files.");
        }

        return request switch
        {
            AddRequest add => await EvaluateAddAsync(
                add, context, invocation_directory, cancellation_token),
            AddAllRequest add_all => await EvaluateAddAllAsync(add_all, context, cancellation_token),
            CommitRequest => Allowed(request, context, "Commit is permitted on this feature branch."),
            SwitchRequest switch_request => await EvaluateSwitchAsync(
                switch_request, context, operation_policy, cancellation_token),
            SwitchCreateRequest create_request => await EvaluateSwitchCreateAsync(
                create_request, context, operation_policy, cancellation_token),
            RebaseBaseRequest rebase_request => await EvaluateRebaseBaseAsync(
                rebase_request, context, cancellation_token),
            BranchDeleteRequest delete_request => await EvaluateBranchDeleteAsync(
                delete_request, context, operation_policy, cancellation_token),
            _ => throw new ArgumentOutOfRangeException(nameof(request), request, "Unknown mutation request."),
        };
    }

    private async Task<EvaluatedOperation> EvaluateAddAsync(
        AddRequest request,
        RepositoryContext context,
        string invocation_directory,
        CancellationToken cancellation_token)
    {
        var resolved_paths = new List<string>();
        foreach (var input in request.Paths)
        {
            if (string.IsNullOrWhiteSpace(input) || input.IndexOf('\0') >= 0)
            {
                return Denied(request, context, "Add paths must be non-empty literal filesystem paths.");
            }

            var absolute = Path.GetFullPath(input, invocation_directory);
            if (!IsWithin(context.State.WorktreeRoot, absolute))
            {
                return Denied(request, context, $"Path '{input}' resolves outside the current worktree.");
            }

            if (IsWithin(context.State.GitDirectory, absolute) || IsWithin(context.State.CommonGitDirectory, absolute))
            {
                return Denied(request, context, $"Path '{input}' resolves inside Git administrative data.");
            }

            resolved_paths.Add(Path.GetRelativePath(context.State.WorktreeRoot, absolute));
        }

        var unsafe_paths = await discovery.FindUnsafeAddPathsAsync(context, resolved_paths, cancellation_token);
        if (unsafe_paths.Count > 0)
        {
            return Denied(request, context,
                $"Staging would traverse a linked directory: {string.Join(", ", unsafe_paths)}.");
        }

        return new EvaluatedOperation(
            request,
            context,
            PolicyDecision.Allow("Selected paths may be staged on this feature branch."),
            null,
            null,
            resolved_paths);
    }

    private async Task<EvaluatedOperation> EvaluateAddAllAsync(
        AddAllRequest request,
        RepositoryContext context,
        CancellationToken cancellation_token)
    {
        var unsafe_paths = await discovery.FindUnsafeAddPathsAsync(context, ["."], cancellation_token);
        return unsafe_paths.Count > 0
            ? Denied(request, context,
                $"Staging would traverse a linked directory: {string.Join(", ", unsafe_paths)}.")
            : Allowed(request, context, "Staging all changes is permitted on this feature branch.");
    }

    private async Task<EvaluatedOperation> EvaluateSwitchAsync(
        SwitchRequest request,
        RepositoryContext context,
        OperationPolicy operation_policy,
        CancellationToken cancellation_token)
    {
        var target = await discovery.FindBranchAsync(context, request.Branch, cancellation_token);
        if (target is null)
        {
            return Denied(request, context, $"Local branch '{request.Branch}' does not exist.");
        }

        if (!operation_policy.AllowedTargetGroups.Contains(target.Classification))
        {
            return Denied(request, context,
                $"Switching to {target.Classification.ToString().ToLowerInvariant()} branch '{target.Name}' is forbidden.",
                target);
        }

        if (target.Classification == BranchClassification.Workspace &&
            !string.Equals(target.Name, context.State.HomeBranch, StringComparison.OrdinalIgnoreCase))
        {
            return Denied(request, context,
                $"Workspace branch '{target.Name}' is not the home branch for this worktree.", target);
        }

        if (target.Worktree is not null && !PathsEqual(target.Worktree.Path, context.State.WorktreeRoot))
        {
            return Denied(request, context,
                $"Branch '{target.Name}' is checked out in worktree '{target.Worktree.Path}'.", target);
        }

        var unsafe_paths = await discovery.FindUnsafeCheckoutPathsAsync(
            context,
            target.Commit,
            cancellation_token);
        if (unsafe_paths.Count > 0)
        {
            return Denied(request, context,
                $"Switching would overwrite an untracked/ignored path or traverse a linked directory: " +
                $"{string.Join(", ", unsafe_paths)}.", target);
        }

        return Allowed(request, context, $"Clean worktree may switch to branch '{target.Name}'.", target);
    }

    private async Task<EvaluatedOperation> EvaluateSwitchCreateAsync(
        SwitchCreateRequest request,
        RepositoryContext context,
        OperationPolicy operation_policy,
        CancellationToken cancellation_token)
    {
        await discovery.ValidateBranchNameAsync(context.State.WorktreeRoot, request.Branch, cancellation_token);
        var existing = await discovery.FindBranchAsync(context, request.Branch, cancellation_token);
        if (existing is not null)
        {
            return Denied(request, context, $"Local branch '{request.Branch}' already exists.", existing);
        }

        var classification = context.Policy.Classify(request.Branch);
        if (!operation_policy.AllowedTargetGroups.Contains(classification))
        {
            return Denied(request, context,
                $"New branch '{request.Branch}' would be classified as {classification.ToString().ToLowerInvariant()}.");
        }

        return Allowed(request, context,
            $"New feature branch '{request.Branch}' may be created without discarding local changes.");
    }

    private async Task<EvaluatedOperation> EvaluateRebaseBaseAsync(
        RebaseBaseRequest request,
        RepositoryContext context,
        CancellationToken cancellation_token)
    {
        var unsafe_paths = await discovery.FindUnsafeCheckoutPathsAsync(
            context,
            context.State.BaseCommit,
            cancellation_token);
        if (unsafe_paths.Count > 0)
        {
            return Denied(request, context,
                $"Rebase would overwrite an untracked/ignored path or traverse a linked directory: " +
                $"{string.Join(", ", unsafe_paths)}.");
        }

        return Allowed(request, context,
            $"Feature branch may be rebased onto configured base '{context.Policy.BaseBranch}'.");
    }

    private async Task<EvaluatedOperation> EvaluateBranchDeleteAsync(
        BranchDeleteRequest request,
        RepositoryContext context,
        OperationPolicy operation_policy,
        CancellationToken cancellation_token)
    {
        var target = await discovery.FindBranchAsync(context, request.Branch, cancellation_token);
        if (target is null)
        {
            return Denied(request, context, $"Local branch '{request.Branch}' does not exist.");
        }

        if (!operation_policy.AllowedTargetGroups.Contains(target.Classification))
        {
            return Denied(request, context,
                $"Deletion of {target.Classification.ToString().ToLowerInvariant()} branch '{target.Name}' is forbidden.",
                target);
        }

        if (string.Equals(target.Name, context.State.CurrentBranch, StringComparison.OrdinalIgnoreCase))
        {
            return Denied(request, context, $"Current branch '{target.Name}' cannot be deleted.", target);
        }

        if (target.Worktree is not null)
        {
            return Denied(request, context,
                $"Branch '{target.Name}' is checked out in worktree '{target.Worktree.Path}'.", target);
        }

        var merged = await discovery.IsAncestorAsync(
            context,
            $"refs/heads/{target.Name}",
            $"refs/heads/{context.Policy.BaseBranch}",
            cancellation_token);
        if (!merged)
        {
            return Denied(request, context,
                $"Branch '{target.Name}' is not fully merged into base branch '{context.Policy.BaseBranch}'.", target);
        }

        var base_worktrees = context.State.Worktrees.Where(worktree =>
            string.Equals(worktree.Branch, context.Policy.BaseBranch, StringComparison.OrdinalIgnoreCase)).ToArray();
        if (base_worktrees.Length != 1)
        {
            return Denied(request, context,
                $"Safe deletion requires base branch '{context.Policy.BaseBranch}' to be checked out in exactly one worktree.",
                target);
        }

        return new EvaluatedOperation(
            request,
            context,
            PolicyDecision.Allow(
                $"Feature branch '{target.Name}' is merged into '{context.Policy.BaseBranch}' and may be safely deleted."),
            target,
            base_worktrees[0],
            null);
    }

    private static EvaluatedOperation Allowed(
        MutationRequest request,
        RepositoryContext context,
        string reason,
        TargetBranchState? target = null)
    {
        return new EvaluatedOperation(request, context, PolicyDecision.Allow(reason), target, null, null);
    }

    private static EvaluatedOperation Denied(
        MutationRequest request,
        RepositoryContext context,
        string reason,
        TargetBranchState? target = null)
    {
        return new EvaluatedOperation(request, context, PolicyDecision.Deny(reason), target, null, null);
    }

    private static bool IsWithin(string root, string path)
    {
        var relative = Path.GetRelativePath(root, path);
        return !Path.IsPathFullyQualified(relative) &&
            !relative.Equals("..", StringComparison.Ordinal) &&
            !relative.StartsWith($"..{Path.DirectorySeparatorChar}", StringComparison.Ordinal);
    }

    private static bool PathsEqual(string left, string right)
    {
        return string.Equals(
            Path.GetFullPath(left).TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar),
            Path.GetFullPath(right).TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar),
            OperatingSystem.IsWindows() ? StringComparison.OrdinalIgnoreCase : StringComparison.Ordinal);
    }
}
