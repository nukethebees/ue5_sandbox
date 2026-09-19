using System.Reflection;
using System.Text;
using System.Text.Json;

namespace AgentGit;

internal sealed class AgentGitApplication(
    TrustContext trust,
    RepositoryDiscovery discovery,
    PolicyEvaluator evaluator,
    OperationExecutor executor,
    TextWriter standard_output,
    TextWriter standard_error)
{
    public async Task<int> RunAsync(
        AgentGitRequest request,
        string working_directory,
        CancellationToken cancellation_token = default)
    {
        try
        {
            switch (request)
            {
                case HelpRequest:
                    standard_output.WriteLine(CommandLine.Usage);
                    return ExitCodes.Success;
                case VersionRequest:
                    standard_output.WriteLine(
                        Assembly.GetExecutingAssembly().GetName().Version?.ToString() ?? "unknown");
                    return ExitCodes.Success;
                case StatusRequest:
                    return await ShowStatusAsync(working_directory, cancellation_token);
                case BranchInfoRequest:
                    return await ShowBranchInfoAsync(working_directory, cancellation_token);
                case PolicyRequest policy_request:
                    return await ShowPolicyAsync(policy_request, working_directory, cancellation_token);
                case MutationRequest mutation:
                    return await MutateAsync(mutation, working_directory, cancellation_token);
                default:
                    throw new ArgumentOutOfRangeException(nameof(request), request, "Unknown agent-git request.");
            }
        }
        catch (AgentGitException exception)
        {
            standard_error.WriteLine($"agent-git: {exception.Message}");
            return exception.ExitCode;
        }
        catch (OperationCanceledException)
        {
            standard_error.WriteLine("agent-git: operation cancelled.");
            return ExitCodes.StateFailure;
        }
        catch (Exception exception)
        {
            standard_error.WriteLine($"agent-git: unexpected failure: {exception.Message}");
            return ExitCodes.UnexpectedFailure;
        }
    }

    private async Task<int> ShowStatusAsync(string working_directory, CancellationToken cancellation_token)
    {
        var context = await discovery.DiscoverAsync(trust, working_directory, cancellation_token);
        WriteRepository(context);
        standard_output.WriteLine($"Staged changes: {YesNo(context.State.Status.HasStagedChanges)}");
        standard_output.WriteLine($"Unstaged changes: {YesNo(context.State.Status.HasUnstagedChanges)}");
        standard_output.WriteLine($"Untracked files: {YesNo(context.State.Status.HasUntrackedFiles)}");
        standard_output.WriteLine($"Conflicts: {YesNo(context.State.Status.HasConflicts)}");
        standard_output.WriteLine($"Git operation: {context.State.OperationState.ToString().ToLowerInvariant()}");
        return ExitCodes.Success;
    }

    private async Task<int> ShowBranchInfoAsync(string working_directory, CancellationToken cancellation_token)
    {
        var context = await discovery.DiscoverAsync(trust, working_directory, cancellation_token);
        WriteRepository(context);
        standard_output.WriteLine($"Base branch: {context.Policy.BaseBranch}");
        standard_output.WriteLine($"Base commit: {context.State.BaseCommit}");
        standard_output.WriteLine($"Policy commit: {context.State.PolicyCommit}");
        standard_output.WriteLine($"Home branch: {context.State.HomeBranch ?? "none"}");
        standard_output.WriteLine("Worktrees:");
        foreach (var worktree in context.State.Worktrees)
        {
            standard_output.WriteLine($"  {worktree.Path}: {worktree.Branch ?? "detached"}");
        }

        return ExitCodes.Success;
    }

    private async Task<int> ShowPolicyAsync(
        PolicyRequest request,
        string working_directory,
        CancellationToken cancellation_token)
    {
        var context = await discovery.DiscoverAsync(trust, working_directory, cancellation_token);
        var mutation = CreatePolicyProbe(request);
        var evaluated = await evaluator.EvaluateAsync(
            mutation,
            context,
            Path.GetFullPath(working_directory),
            cancellation_token);
        WriteRepository(context);
        WriteDecision(evaluated);
        return evaluated.Decision.Allowed ? ExitCodes.Success : ExitCodes.PolicyDenied;
    }

    private async Task<int> MutateAsync(
        MutationRequest request,
        string working_directory,
        CancellationToken cancellation_token)
    {
        var initial_context = await discovery.DiscoverAsync(trust, working_directory, cancellation_token);
        if (request.DryRun)
        {
            var dry_evaluation = await evaluator.EvaluateAsync(
                request,
                initial_context,
                Path.GetFullPath(working_directory),
                cancellation_token);
            WriteRepository(initial_context);
            WriteDecision(dry_evaluation);
            if (dry_evaluation.Decision.Allowed)
            {
                WriteInvocation(dry_evaluation);
                standard_output.WriteLine("Dry run: no mutation executed.");
                return ExitCodes.Success;
            }

            return ExitCodes.PolicyDenied;
        }

        await using var repository_lock = await RepositoryLock.AcquireAsync(
            initial_context.State.CommonGitDirectory,
            cancellation_token);
        var context = await discovery.DiscoverAsync(trust, working_directory, cancellation_token);
        if (!PathsEqual(context.State.CommonGitDirectory, initial_context.State.CommonGitDirectory))
        {
            throw new RepositoryStateException("Repository identity changed while acquiring the agent-git mutation lock.");
        }

        var evaluated = await evaluator.EvaluateAsync(
            request,
            context,
            Path.GetFullPath(working_directory),
            cancellation_token);
        WriteRepository(context);
        WriteDecision(evaluated);
        if (!evaluated.Decision.Allowed)
        {
            return ExitCodes.PolicyDenied;
        }

        WriteInvocation(evaluated);
        var result = await executor.ExecuteAsync(evaluated, cancellation_token);
        if (result.StandardOutput.Length > 0)
        {
            standard_output.Write(Encoding.UTF8.GetString(result.StandardOutput));
        }

        if (!string.IsNullOrEmpty(result.StandardError))
        {
            standard_error.Write(result.StandardError);
        }

        GitClient.EnsureSuccess(result, OperationExecutor.DiagnosticArguments(evaluated));
        return ExitCodes.Success;
    }

    private void WriteRepository(RepositoryContext context)
    {
        standard_output.WriteLine($"Repository: {context.Registration.RepositoryId}");
        standard_output.WriteLine($"Worktree: {context.State.WorktreeRoot}");
        standard_output.WriteLine($"Current branch: {context.State.CurrentBranch ?? "detached"}");
        standard_output.WriteLine(
            $"Classification: {context.State.CurrentClassification?.ToString().ToLowerInvariant() ?? "detached"}");
    }

    private void WriteDecision(EvaluatedOperation operation)
    {
        standard_output.WriteLine();
        standard_output.WriteLine($"Operation: {CommandLine.OperationName(operation.Request.Operation)}");
        standard_output.WriteLine($"Decision: {(operation.Decision.Allowed ? "ALLOW" : "DENY")}");
        standard_output.WriteLine();
        standard_output.WriteLine("Reason:");
        standard_output.WriteLine($"  {operation.Decision.Reason}");
    }

    private void WriteInvocation(EvaluatedOperation operation)
    {
        standard_output.WriteLine();
        standard_output.WriteLine("agent-git: executing approved Git operation");
        standard_output.WriteLine(
            $"git argv: {JsonSerializer.Serialize(OperationExecutor.DiagnosticArguments(operation))}");
    }

    private static MutationRequest CreatePolicyProbe(PolicyRequest request)
    {
        return request.Operation switch
        {
            AgentGitOperation.Add => new AddRequest(true, ["."]),
            AgentGitOperation.AddAll => new AddAllRequest(true),
            AgentGitOperation.Commit => new CommitRequest(true, "policy probe"),
            AgentGitOperation.Switch => new SwitchRequest(true, request.Target!),
            AgentGitOperation.SwitchCreate => new SwitchCreateRequest(true, request.Target!),
            AgentGitOperation.RebaseBase => new RebaseBaseRequest(true),
            AgentGitOperation.BranchDelete => new BranchDeleteRequest(true, request.Target!),
            _ => throw new ArgumentOutOfRangeException(nameof(request), request, "Unknown policy probe."),
        };
    }

    private static string YesNo(bool value) => value ? "yes" : "no";

    private static bool PathsEqual(string left, string right)
    {
        return string.Equals(
            Path.GetFullPath(left).TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar),
            Path.GetFullPath(right).TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar),
            OperatingSystem.IsWindows() ? StringComparison.OrdinalIgnoreCase : StringComparison.Ordinal);
    }
}
