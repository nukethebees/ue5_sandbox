using GitSupport;

namespace AgentGit;

internal enum AgentGitOperation
{
    Add,
    AddAll,
    Commit,
    Switch,
    SwitchCreate,
    RebaseBase,
    BranchDelete,
}

internal enum BranchClassification
{
    Protected,
    Workspace,
    Feature,
}

internal enum RepositoryOperationState
{
    None,
    Merge,
    Rebase,
    CherryPick,
    Revert,
    Bisect,
    Sequencer,
}

internal abstract record AgentGitRequest;

internal sealed record HelpRequest : AgentGitRequest;

internal sealed record VersionRequest : AgentGitRequest;

internal sealed record StatusRequest : AgentGitRequest;

internal sealed record BranchInfoRequest : AgentGitRequest;

internal sealed record PolicyRequest(AgentGitOperation Operation, string? Target) : AgentGitRequest;

internal abstract record MutationRequest(bool DryRun, AgentGitOperation Operation) : AgentGitRequest;

internal sealed record AddRequest(bool DryRun, IReadOnlyList<string> Paths)
    : MutationRequest(DryRun, AgentGitOperation.Add);

internal sealed record AddAllRequest(bool DryRun)
    : MutationRequest(DryRun, AgentGitOperation.AddAll);

internal sealed record CommitRequest(bool DryRun, string Message)
    : MutationRequest(DryRun, AgentGitOperation.Commit);

internal sealed record SwitchRequest(bool DryRun, string Branch)
    : MutationRequest(DryRun, AgentGitOperation.Switch);

internal sealed record SwitchCreateRequest(bool DryRun, string Branch)
    : MutationRequest(DryRun, AgentGitOperation.SwitchCreate);

internal sealed record RebaseBaseRequest(bool DryRun)
    : MutationRequest(DryRun, AgentGitOperation.RebaseBase);

internal sealed record BranchDeleteRequest(bool DryRun, string Branch)
    : MutationRequest(DryRun, AgentGitOperation.BranchDelete);

internal sealed record WorkingTreeStatus(
    bool HasStagedChanges,
    bool HasUnstagedChanges,
    bool HasUntrackedFiles,
    bool HasConflicts)
{
    public bool IsClean => !HasStagedChanges && !HasUnstagedChanges && !HasUntrackedFiles && !HasConflicts;
}

internal sealed record RepositoryState(
    string WorktreeRoot,
    string GitDirectory,
    string CommonGitDirectory,
    string HeadCommit,
    string? CurrentBranch,
    BranchClassification? CurrentClassification,
    string BaseCommit,
    string PolicyCommit,
    string MutationFingerprint,
    WorkingTreeStatus Status,
    RepositoryOperationState OperationState,
    IReadOnlyList<Worktree> Worktrees,
    string? HomeBranch);

internal sealed record RepositoryContext(
    TrustedRepository Registration,
    GitPolicy Policy,
    RepositoryState State);

internal sealed record TargetBranchState(
    string Name,
    string Commit,
    BranchClassification Classification,
    Worktree? Worktree);

internal sealed record PolicyDecision(bool Allowed, string Reason)
{
    public static PolicyDecision Allow(string reason) => new(true, reason);

    public static PolicyDecision Deny(string reason) => new(false, reason);
}

internal sealed record EvaluatedOperation(
    MutationRequest Request,
    RepositoryContext Context,
    PolicyDecision Decision,
    TargetBranchState? TargetBranch,
    Worktree? BaseWorktree,
    IReadOnlyList<string>? ResolvedPaths);

internal static class ExitCodes
{
    public const int Success = 0;
    public const int UnexpectedFailure = 1;
    public const int Usage = 2;
    public const int RepositoryFailure = 3;
    public const int ConfigurationFailure = 4;
    public const int PolicyDenied = 5;
    public const int GitFailure = 6;
    public const int StateFailure = 7;
}

internal class AgentGitException(string message, int exit_code, Exception? inner_exception = null)
    : Exception(message, inner_exception)
{
    public int ExitCode { get; } = exit_code;
}

internal sealed class RepositoryException(string message, Exception? inner_exception = null)
    : AgentGitException(message, ExitCodes.RepositoryFailure, inner_exception);

internal sealed class PolicyConfigurationException(string message, Exception? inner_exception = null)
    : AgentGitException(message, ExitCodes.ConfigurationFailure, inner_exception);

internal sealed class GitCommandException(string message, int git_exit_code, string standard_error)
    : AgentGitException(message, ExitCodes.GitFailure)
{
    public int GitExitCode { get; } = git_exit_code;

    public string StandardError { get; } = standard_error;
}

internal sealed class RepositoryStateException(string message, Exception? inner_exception = null)
    : AgentGitException(message, ExitCodes.StateFailure, inner_exception);
