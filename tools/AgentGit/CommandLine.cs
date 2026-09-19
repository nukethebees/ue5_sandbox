namespace AgentGit;

internal static class CommandLine
{
    public const string Usage = """
        Usage:
          agent-git status
          agent-git branch-info
          agent-git policy <add|add-all|commit|switch|switch-create|rebase-base|branch-delete> [branch]
          agent-git [--dry-run] add <path>...
          agent-git [--dry-run] add-all
          agent-git [--dry-run] commit -m <message>
          agent-git [--dry-run] switch <branch>
          agent-git [--dry-run] switch-create <branch>
          agent-git [--dry-run] rebase-base
          agent-git [--dry-run] branch-delete <branch>
        """;

    public static bool TryParse(IReadOnlyList<string> arguments, out AgentGitRequest? request, out string? error)
    {
        ArgumentNullException.ThrowIfNull(arguments);

        request = null;
        error = null;
        if (arguments.Count == 0 || arguments is ["--help"] or ["-h"])
        {
            request = new HelpRequest();
            return true;
        }

        if (arguments is ["--version"])
        {
            request = new VersionRequest();
            return true;
        }

        var index = 0;
        var dry_run = false;
        if (string.Equals(arguments[index], "--dry-run", StringComparison.Ordinal))
        {
            dry_run = true;
            ++index;
        }

        if (index >= arguments.Count)
        {
            error = "--dry-run requires a mutating operation.";
            return false;
        }

        var command = arguments[index++];
        var remaining = arguments.Skip(index).ToArray();
        if (dry_run && command is "status" or "branch-info" or "policy")
        {
            error = "--dry-run is only valid for mutating operations.";
            return false;
        }

        switch (command)
        {
            case "status" when remaining.Length == 0:
                request = new StatusRequest();
                return true;
            case "branch-info" when remaining.Length == 0:
                request = new BranchInfoRequest();
                return true;
            case "policy":
                return TryParsePolicy(remaining, out request, out error);
            case "add" when remaining.Length > 0:
                request = new AddRequest(dry_run, remaining);
                return true;
            case "add-all" when remaining.Length == 0:
                request = new AddAllRequest(dry_run);
                return true;
            case "commit" when remaining is ["-m", var message] && !string.IsNullOrWhiteSpace(message):
                request = new CommitRequest(dry_run, message);
                return true;
            case "switch" when remaining is [var branch] && !string.IsNullOrWhiteSpace(branch):
                request = new SwitchRequest(dry_run, branch);
                return true;
            case "switch-create" when remaining is [var branch] && !string.IsNullOrWhiteSpace(branch):
                request = new SwitchCreateRequest(dry_run, branch);
                return true;
            case "rebase-base" when remaining.Length == 0:
                request = new RebaseBaseRequest(dry_run);
                return true;
            case "branch-delete" when remaining is [var branch] && !string.IsNullOrWhiteSpace(branch):
                request = new BranchDeleteRequest(dry_run, branch);
                return true;
            default:
                error = $"Unknown or invalid operation '{command}'.";
                return false;
        }
    }

    private static bool TryParsePolicy(
        IReadOnlyList<string> arguments,
        out AgentGitRequest? request,
        out string? error)
    {
        request = null;
        error = null;
        if (arguments.Count is < 1 or > 2 || !TryParseOperation(arguments[0], out var operation))
        {
            error = "policy requires a supported operation and, for branch operations, its target branch.";
            return false;
        }

        var requires_target = operation is AgentGitOperation.Switch or AgentGitOperation.SwitchCreate or AgentGitOperation.BranchDelete;
        if (requires_target != (arguments.Count == 2))
        {
            error = requires_target
                ? $"policy {arguments[0]} requires a target branch."
                : $"policy {arguments[0]} does not accept a target.";
            return false;
        }

        request = new PolicyRequest(operation, arguments.Count == 2 ? arguments[1] : null);
        return true;
    }

    internal static bool TryParseOperation(string value, out AgentGitOperation operation)
    {
        var parsed = value switch
        {
            "add" => AgentGitOperation.Add,
            "add-all" => AgentGitOperation.AddAll,
            "commit" => AgentGitOperation.Commit,
            "switch" => AgentGitOperation.Switch,
            "switch-create" => AgentGitOperation.SwitchCreate,
            "rebase-base" => AgentGitOperation.RebaseBase,
            "branch-delete" => AgentGitOperation.BranchDelete,
            _ => (AgentGitOperation?)null,
        };
        operation = parsed ?? default;
        return parsed is not null;
    }

    internal static string OperationName(AgentGitOperation operation)
    {
        return operation switch
        {
            AgentGitOperation.Add => "add",
            AgentGitOperation.AddAll => "add-all",
            AgentGitOperation.Commit => "commit",
            AgentGitOperation.Switch => "switch",
            AgentGitOperation.SwitchCreate => "switch-create",
            AgentGitOperation.RebaseBase => "rebase-base",
            AgentGitOperation.BranchDelete => "branch-delete",
            _ => throw new ArgumentOutOfRangeException(nameof(operation), operation, "Unknown agent-git operation."),
        };
    }
}
