using System.Text;
using GitSupport;

namespace AgentGit;

internal sealed class RepositoryDiscovery(GitClient git)
{
    public async Task<RepositoryContext> DiscoverAsync(
        TrustContext trust,
        string start_directory,
        CancellationToken cancellation_token = default)
    {
        var working_directory = Path.GetFullPath(start_directory);
        if (!Directory.Exists(working_directory))
        {
            throw new RepositoryException($"Working directory does not exist: '{working_directory}'.");
        }

        var root = await DiscoverPathAsync(working_directory, "--show-toplevel", cancellation_token);
        var git_directory = await DiscoverPathAsync(working_directory, "--git-dir", cancellation_token);
        var common_git_directory = await DiscoverPathAsync(working_directory, "--git-common-dir", cancellation_token);
        if (!IsWithin(root, working_directory))
        {
            throw new RepositoryException(
                $"Working directory '{working_directory}' is not within discovered worktree '{root}'.");
        }

        var registration = trust.FindRegistration(common_git_directory);
        var origin_url = await git.RequireTextAsync(
            root,
            ["config", "--local", "--get", "remote.origin.url"],
            cancellation_token);
        if (!string.Equals(origin_url, registration.OriginUrl, StringComparison.Ordinal))
        {
            throw new RepositoryException(
                $"Repository origin '{origin_url}' does not match trusted origin '{registration.OriginUrl}'.");
        }

        await ValidateRepositoryConfigurationAsync(root, common_git_directory, cancellation_token);
        var policy_commit = await git.RequireTextAsync(
            root,
            ["rev-parse", "--verify", $"{registration.PolicyRef}^{{commit}}"],
            cancellation_token);
        var policy_json = await git.RequireTextAsync(
            root,
            ["cat-file", "blob", $"{registration.PolicyRef}:{registration.PolicyPath}"],
            cancellation_token);
        if (Encoding.UTF8.GetByteCount(policy_json) > 64 * 1024)
        {
            throw new PolicyConfigurationException("Policy document exceeds the 64 KiB safety limit.");
        }

        var policy = PolicyLoader.Parse(policy_json);
        if (!string.Equals(policy.RepositoryId, registration.RepositoryId, StringComparison.Ordinal))
        {
            throw new PolicyConfigurationException(
                $"Policy repositoryId '{policy.RepositoryId}' does not match trusted identity '{registration.RepositoryId}'.");
        }

        var expected_policy_ref = $"refs/heads/{policy.BaseBranch}";
        if (!string.Equals(expected_policy_ref, registration.PolicyRef, StringComparison.Ordinal))
        {
            throw new PolicyConfigurationException(
                $"Policy baseBranch '{policy.BaseBranch}' does not match trusted policy ref '{registration.PolicyRef}'.");
        }

        if (policy.GitExtensions.Contains("lfs") != (trust.GitLfsExecutable is not null))
        {
            throw new PolicyConfigurationException(
                "Policy and trust manifest disagree about whether Git LFS is enabled.");
        }

        var head = await git.RequireTextAsync(root, ["rev-parse", "--verify", "HEAD^{commit}"], cancellation_token);
        var branch_result = await git.RunAsync(root, ["symbolic-ref", "--quiet", "--short", "HEAD"], cancellation_token: cancellation_token);
        string? current_branch = branch_result.ExitCode switch
        {
            0 => Encoding.UTF8.GetString(branch_result.StandardOutput).TrimEnd('\r', '\n', '\0'),
            1 => null,
            _ => throw CreateGitFailure("symbolic-ref", branch_result),
        };
        BranchClassification? classification = current_branch is null ? null : policy.Classify(current_branch);
        var base_commit = await git.RequireTextAsync(
            root,
            ["rev-parse", "--verify", $"refs/heads/{policy.BaseBranch}^{{commit}}"],
            cancellation_token);
        var status = await DiscoverStatusAsync(root, cancellation_token);
        var worktrees = await DiscoverWorktreesAsync(root, cancellation_token);
        var current_worktree = worktrees.SingleOrDefault(worktree => PathsEqual(worktree.Path, root));
        if (current_worktree is null)
        {
            throw new RepositoryException($"Git worktree registry does not contain current worktree '{root}'.");
        }

        var home_branch = await FindHomeBranchAsync(root, policy, cancellation_token);
        var operation_state = DiscoverOperationState(git_directory);
        var state = new RepositoryState(
            root,
            git_directory,
            common_git_directory,
            head,
            current_branch,
            classification,
            base_commit,
            policy_commit,
            status,
            operation_state,
            worktrees,
            home_branch);
        return new RepositoryContext(registration, policy, state);
    }

    public async Task<TargetBranchState?> FindBranchAsync(
        RepositoryContext context,
        string branch,
        CancellationToken cancellation_token = default)
    {
        await ValidateBranchNameAsync(context.State.WorktreeRoot, branch, cancellation_token);
        var reference = $"refs/heads/{branch}";
        var exists = await git.RunAsync(
            context.State.WorktreeRoot,
            ["show-ref", "--verify", "--quiet", reference],
            cancellation_token: cancellation_token);
        if (exists.ExitCode == 1)
        {
            return null;
        }

        GitClient.EnsureSuccess(exists, ["show-ref"]);
        var symref = await git.RunAsync(
            context.State.WorktreeRoot,
            ["symbolic-ref", "--quiet", reference],
            cancellation_token: cancellation_token);
        if (symref.ExitCode == 0)
        {
            throw new RepositoryStateException($"Branch '{branch}' is a symbolic ref and is not safe for agent-git.");
        }

        if (symref.ExitCode != 1)
        {
            GitClient.EnsureSuccess(symref, ["symbolic-ref"]);
        }

        var commit = await git.RequireTextAsync(
            context.State.WorktreeRoot,
            ["rev-parse", "--verify", $"{reference}^{{commit}}"],
            cancellation_token);
        var owning_worktree = context.State.Worktrees.SingleOrDefault(worktree =>
            string.Equals(worktree.Branch, branch, StringComparison.OrdinalIgnoreCase));
        return new TargetBranchState(branch, commit, context.Policy.Classify(branch), owning_worktree);
    }

    public async Task ValidateBranchNameAsync(
        string worktree_root,
        string branch,
        CancellationToken cancellation_token = default)
    {
        if (string.IsNullOrWhiteSpace(branch) || branch[0] == '-')
        {
            throw new RepositoryStateException($"Invalid local branch name '{branch}'.");
        }

        var result = await git.RunAsync(
            worktree_root,
            ["check-ref-format", $"refs/heads/{branch}"],
            cancellation_token: cancellation_token);
        if (result.ExitCode != 0)
        {
            throw new RepositoryStateException($"Invalid local branch name '{branch}'.");
        }
    }

    public async Task<bool> IsAncestorAsync(
        RepositoryContext context,
        string ancestor,
        string descendant,
        CancellationToken cancellation_token = default)
    {
        var result = await git.RunAsync(
            context.State.WorktreeRoot,
            ["merge-base", "--is-ancestor", ancestor, descendant],
            cancellation_token: cancellation_token);
        return result.ExitCode switch
        {
            0 => true,
            1 => false,
            _ => throw CreateGitFailure("merge-base", result),
        };
    }

    private async Task<string> DiscoverPathAsync(
        string working_directory,
        string selector,
        CancellationToken cancellation_token)
    {
        ProcessResult result;
        try
        {
            result = await git.RunAsync(
                working_directory,
                ["rev-parse", "--path-format=absolute", selector],
                cancellation_token: cancellation_token);
        }
        catch (RepositoryException)
        {
            throw;
        }

        if (result.ExitCode != 0)
        {
            var diagnostic = string.IsNullOrWhiteSpace(result.StandardError)
                ? "Git did not identify a repository."
                : result.StandardError.Trim();
            throw new RepositoryException($"Repository discovery failed: {diagnostic}");
        }

        var path = Encoding.UTF8.GetString(result.StandardOutput).TrimEnd('\r', '\n', '\0');
        if (string.IsNullOrWhiteSpace(path))
        {
            throw new RepositoryException($"Repository discovery returned an empty value for {selector}.");
        }

        return Path.GetFullPath(path);
    }

    private async Task ValidateRepositoryConfigurationAsync(
        string worktree_root,
        string common_git_directory,
        CancellationToken cancellation_token)
    {
        var dangerous_pattern = "^(include\\.|includeif\\.|filter\\..*\\.(clean|smudge|process)|merge\\..*\\.driver)$";
        var scopes = new List<string> { "--local" };
        var worktree_config = await git.RunAsync(
            worktree_root,
            ["config", "--local", "--bool", "--get", "extensions.worktreeConfig"],
            cancellation_token: cancellation_token);
        if (worktree_config.ExitCode == 0 &&
            string.Equals(
                Encoding.UTF8.GetString(worktree_config.StandardOutput).Trim(),
                "true",
                StringComparison.OrdinalIgnoreCase))
        {
            scopes.Add("--worktree");
        }
        else if (worktree_config.ExitCode is not (0 or 1))
        {
            GitClient.EnsureSuccess(worktree_config, ["config"]);
        }

        foreach (var scope in scopes)
        {
            var result = await git.RunAsync(
                worktree_root,
                ["config", scope, "--name-only", "--get-regexp", dangerous_pattern],
                cancellation_token: cancellation_token);
            if (result.ExitCode == 1)
            {
                continue;
            }

            GitClient.EnsureSuccess(result, ["config"]);
            var names = Encoding.UTF8.GetString(result.StandardOutput)
                .Split(['\r', '\n'], StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries);
            var unknown = names.Where(name => !name.StartsWith("filter.lfs.", StringComparison.OrdinalIgnoreCase)).ToArray();
            if (unknown.Length > 0)
            {
                throw new RepositoryStateException(
                    $"Repository Git configuration contains unsupported executable settings: {string.Join(", ", unknown)}.");
            }
        }

        var alternates = Path.Combine(common_git_directory, "objects", "info", "alternates");
        if (File.Exists(alternates))
        {
            throw new RepositoryStateException("Git object alternates are not supported by agent-git.");
        }
    }

    private async Task<WorkingTreeStatus> DiscoverStatusAsync(
        string worktree_root,
        CancellationToken cancellation_token)
    {
        var result = await git.RunAsync(
            worktree_root,
            ["status", "--porcelain=v2", "-z", "--untracked-files=all", "--ignore-submodules=none"],
            cancellation_token: cancellation_token);
        GitClient.EnsureSuccess(result, ["status"]);

        var staged = false;
        var unstaged = false;
        var untracked = false;
        var conflicts = false;
        var records = Encoding.UTF8.GetString(result.StandardOutput).Split('\0');
        for (var index = 0; index < records.Length; ++index)
        {
            var record = records[index];
            if (record.Length == 0)
            {
                continue;
            }

            if (record.StartsWith("? ", StringComparison.Ordinal))
            {
                untracked = true;
                continue;
            }

            if (record.StartsWith("u ", StringComparison.Ordinal))
            {
                conflicts = true;
                staged = true;
                unstaged = true;
                continue;
            }

            if (record.StartsWith("1 ", StringComparison.Ordinal) || record.StartsWith("2 ", StringComparison.Ordinal))
            {
                if (record.Length < 4)
                {
                    throw new RepositoryException($"Git status returned malformed record '{record}'.");
                }

                staged |= record[2] != '.';
                unstaged |= record[3] != '.';
                if (record[0] == '2')
                {
                    ++index;
                }
            }
        }

        return new WorkingTreeStatus(staged, unstaged, untracked, conflicts);
    }

    private async Task<IReadOnlyList<Worktree>> DiscoverWorktreesAsync(
        string worktree_root,
        CancellationToken cancellation_token)
    {
        var result = await git.RunAsync(
            worktree_root,
            ["worktree", "list", "--porcelain", "-z"],
            cancellation_token: cancellation_token);
        GitClient.EnsureSuccess(result, ["worktree"]);
        try
        {
            return WorktreePorcelainParser.ParseNullDelimited(Encoding.UTF8.GetString(result.StandardOutput));
        }
        catch (FormatException exception)
        {
            throw new RepositoryException($"Unable to parse Git worktree state: {exception.Message}", exception);
        }
    }

    private async Task<string?> FindHomeBranchAsync(
        string root,
        GitPolicy policy,
        CancellationToken cancellation_token)
    {
        var directory_name = Path.GetFileName(root.TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar));
        if (string.IsNullOrWhiteSpace(directory_name) || policy.Classify(directory_name) != BranchClassification.Workspace)
        {
            return null;
        }

        var exists = await git.RunAsync(
            root,
            ["show-ref", "--verify", "--quiet", $"refs/heads/{directory_name}"],
            cancellation_token: cancellation_token);
        if (exists.ExitCode == 1)
        {
            return null;
        }

        GitClient.EnsureSuccess(exists, ["show-ref"]);
        return directory_name;
    }

    private static RepositoryOperationState DiscoverOperationState(string git_directory)
    {
        if (Directory.Exists(Path.Combine(git_directory, "rebase-merge")) ||
            Directory.Exists(Path.Combine(git_directory, "rebase-apply")))
        {
            return RepositoryOperationState.Rebase;
        }

        if (File.Exists(Path.Combine(git_directory, "MERGE_HEAD")))
        {
            return RepositoryOperationState.Merge;
        }

        if (File.Exists(Path.Combine(git_directory, "CHERRY_PICK_HEAD")))
        {
            return RepositoryOperationState.CherryPick;
        }

        if (File.Exists(Path.Combine(git_directory, "REVERT_HEAD")))
        {
            return RepositoryOperationState.Revert;
        }

        if (File.Exists(Path.Combine(git_directory, "BISECT_START")))
        {
            return RepositoryOperationState.Bisect;
        }

        return Directory.Exists(Path.Combine(git_directory, "sequencer"))
            ? RepositoryOperationState.Sequencer
            : RepositoryOperationState.None;
    }

    private static GitCommandException CreateGitFailure(string command, ProcessResult result)
    {
        var diagnostic = string.IsNullOrWhiteSpace(result.StandardError)
            ? $"Git exited with code {result.ExitCode}."
            : result.StandardError.Trim();
        return new GitCommandException($"Git {command} failed: {diagnostic}", result.ExitCode, result.StandardError);
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
