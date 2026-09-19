namespace GitSupport;

public sealed record Worktree(
    string Name,
    string Path,
    string? Head,
    string? Branch,
    bool IsBare,
    string? LockedReason,
    string? PrunableReason);
