using System.IO;

namespace GitSupport;

public static class WorktreePorcelainParser
{
    public static IReadOnlyList<Worktree> ParseNullDelimited(string output)
    {
        ArgumentNullException.ThrowIfNull(output);
        return ParseRecords(output.Split('\0'));
    }

    public static IReadOnlyList<Worktree> ParseLines(string output)
    {
        ArgumentNullException.ThrowIfNull(output);
        return ParseRecords(output.Replace("\r\n", "\n", StringComparison.Ordinal).Split('\n'));
    }

    private static IReadOnlyList<Worktree> ParseRecords(IEnumerable<string> records)
    {
        var worktrees = new List<Worktree>();
        string? worktree_path = null;
        string? head = null;
        string? branch = null;
        var is_bare = false;
        string? locked_reason = null;
        string? prunable_reason = null;

        void CompleteWorktree()
        {
            if (worktree_path is null)
            {
                return;
            }

            var trimmed_path = worktree_path.TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar);
            var name = Path.GetFileName(trimmed_path);
            if (string.IsNullOrWhiteSpace(name))
            {
                throw new FormatException($"Git reported a worktree path without a directory name: '{worktree_path}'.");
            }

            worktrees.Add(new Worktree(name, worktree_path, head, branch, is_bare, locked_reason, prunable_reason));
            worktree_path = null;
            head = null;
            branch = null;
            is_bare = false;
            locked_reason = null;
            prunable_reason = null;
        }

        foreach (var record in records)
        {
            if (record.Length == 0)
            {
                CompleteWorktree();
                continue;
            }

            if (record.StartsWith("worktree ", StringComparison.Ordinal))
            {
                CompleteWorktree();
                worktree_path = RequireValue(record, "worktree ");
                continue;
            }

            if (worktree_path is null)
            {
                if (string.IsNullOrWhiteSpace(record))
                {
                    continue;
                }

                throw new FormatException($"Git reported metadata before its worktree path: '{record}'.");
            }

            if (record.StartsWith("HEAD ", StringComparison.Ordinal))
            {
                head = RequireValue(record, "HEAD ");
            }
            else if (record.StartsWith("branch ", StringComparison.Ordinal))
            {
                branch = RequireValue(record, "branch ");
                const string local_branch_prefix = "refs/heads/";
                if (branch.StartsWith(local_branch_prefix, StringComparison.Ordinal))
                {
                    branch = branch[local_branch_prefix.Length..];
                }
            }
            else if (record.Equals("detached", StringComparison.Ordinal))
            {
                branch = null;
            }
            else if (record.Equals("bare", StringComparison.Ordinal))
            {
                is_bare = true;
            }
            else if (record.Equals("locked", StringComparison.Ordinal))
            {
                locked_reason = string.Empty;
            }
            else if (record.StartsWith("locked ", StringComparison.Ordinal))
            {
                locked_reason = record["locked ".Length..];
            }
            else if (record.Equals("prunable", StringComparison.Ordinal))
            {
                prunable_reason = string.Empty;
            }
            else if (record.StartsWith("prunable ", StringComparison.Ordinal))
            {
                prunable_reason = record["prunable ".Length..];
            }
        }

        CompleteWorktree();
        return worktrees;
    }

    private static string RequireValue(string record, string prefix)
    {
        var value = record[prefix.Length..];
        if (string.IsNullOrWhiteSpace(value))
        {
            throw new FormatException($"Git reported empty worktree metadata for '{prefix.Trim()}'.");
        }

        return value;
    }
}
