using System.IO;

namespace GitTools;

public static class WorktreePorcelainParser
{
    public static IReadOnlyList<Worktree> Parse(string output)
    {
        ArgumentNullException.ThrowIfNull(output);

        var worktrees = new List<Worktree>();
        string? worktree_path = null;
        string? branch = null;

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

            worktrees.Add(new Worktree(name, worktree_path, branch));
            worktree_path = null;
            branch = null;
        }

        using var reader = new StringReader(output);
        while (reader.ReadLine() is { } line)
        {
            if (string.IsNullOrWhiteSpace(line))
            {
                CompleteWorktree();
                continue;
            }

            if (line.StartsWith("worktree ", StringComparison.Ordinal))
            {
                CompleteWorktree();

                worktree_path = line["worktree ".Length..];
                if (string.IsNullOrWhiteSpace(worktree_path))
                {
                    throw new FormatException("Git reported an empty worktree path.");
                }

                continue;
            }

            if (line.StartsWith("branch ", StringComparison.Ordinal))
            {
                if (worktree_path is null)
                {
                    throw new FormatException("Git reported a branch before its worktree path.");
                }

                branch = line["branch ".Length..];
                if (string.IsNullOrWhiteSpace(branch))
                {
                    throw new FormatException("Git reported an empty worktree branch.");
                }

                const string local_branch_prefix = "refs/heads/";
                if (branch.StartsWith(local_branch_prefix, StringComparison.Ordinal))
                {
                    branch = branch[local_branch_prefix.Length..];
                }

                continue;
            }

            if (line.Equals("detached", StringComparison.Ordinal) && worktree_path is not null)
            {
                branch = null;
            }
        }

        CompleteWorktree();
        return worktrees;
    }
}
