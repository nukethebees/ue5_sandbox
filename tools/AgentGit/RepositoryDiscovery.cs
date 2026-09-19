using System.Text;
using System.Security.Cryptography;
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
        if (IsReparseDirectory(root))
        {
            throw new RepositoryException($"Worktree root '{root}' is a linked directory, which is not supported.");
        }

        if (!IsWithin(root, working_directory))
        {
            throw new RepositoryException(
                $"Working directory '{working_directory}' is not within discovered worktree '{root}'.");
        }

        var registration = trust.FindRegistration(common_git_directory);
        await ValidateRepositoryConfigurationAsync(
            working_directory,
            git_directory,
            common_git_directory,
            cancellation_token);
        var origin_url = await git.RequireTextAsync(
            root,
            ["config", "--local", "--get", "remote.origin.url"],
            cancellation_token);
        if (!string.Equals(origin_url, registration.OriginUrl, StringComparison.Ordinal))
        {
            throw new RepositoryException("Repository origin does not match the trusted registration.");
        }

        await ValidateIndexFlagsAsync(root, cancellation_token);
        await ValidateDirectRefAsync(root, registration.PolicyRef, cancellation_token);
        string policy_commit;
        string policy_json;
        try
        {
            policy_commit = await git.RequireTextAsync(
                root,
                ["rev-parse", "--verify", $"{registration.PolicyRef}^{{commit}}"],
                cancellation_token);
            var policy_size_text = await git.RequireTextAsync(
                root,
                ["cat-file", "-s", $"{registration.PolicyRef}:{registration.PolicyPath}"],
                cancellation_token);
            if (!long.TryParse(policy_size_text, out var policy_size) || policy_size is < 0 or > 64 * 1024)
            {
                throw new PolicyConfigurationException(
                    $"Policy document has invalid size '{policy_size_text}' or exceeds the 64 KiB safety limit.");
            }

            policy_json = await git.RequireTextAsync(
                root,
                ["cat-file", "blob", $"{registration.PolicyRef}:{registration.PolicyPath}"],
                cancellation_token);
        }
        catch (GitCommandException exception)
        {
            throw new PolicyConfigurationException(
                $"Unable to load policy '{registration.PolicyRef}:{registration.PolicyPath}': {exception.Message}",
                exception);
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
        var branch_result = await git.RunAsync(
            root,
            ["symbolic-ref", "--quiet", "--short", "--no-recurse", "HEAD"],
            cancellation_token: cancellation_token);
        string? current_branch = branch_result.ExitCode switch
        {
            0 => Encoding.UTF8.GetString(branch_result.StandardOutput).TrimEnd('\r', '\n', '\0'),
            1 => null,
            _ => throw CreateGitFailure("symbolic-ref", branch_result),
        };
        if (current_branch is not null)
        {
            await ValidateBranchNameAsync(root, current_branch, cancellation_token);
            await ValidateDirectRefAsync(root, $"refs/heads/{current_branch}", cancellation_token);
        }
        BranchClassification? classification = current_branch is null ? null : policy.Classify(current_branch);

        await ValidateDirectRefAsync(root, $"refs/heads/{policy.BaseBranch}", cancellation_token);
        var base_commit = await git.RequireTextAsync(
            root,
            ["rev-parse", "--verify", $"refs/heads/{policy.BaseBranch}^{{commit}}"],
            cancellation_token);
        var status_snapshot = await DiscoverStatusSnapshotAsync(root, cancellation_token);
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
            status_snapshot.Fingerprint,
            status_snapshot.Status,
            operation_state,
            worktrees,
            home_branch);
        return new RepositoryContext(registration, policy, state);
    }

    public async Task RevalidateMutationSnapshotAsync(
        RepositoryContext context,
        CancellationToken cancellation_token = default)
    {
        var state = context.State;
        await ValidateRepositoryConfigurationAsync(
            state.WorktreeRoot,
            state.GitDirectory,
            state.CommonGitDirectory,
            cancellation_token);
        await ValidateIndexFlagsAsync(state.WorktreeRoot, cancellation_token);
        await ValidateDirectRefAsync(
            state.WorktreeRoot,
            context.Registration.PolicyRef,
            cancellation_token);
        await ValidateDirectRefAsync(
            state.WorktreeRoot,
            $"refs/heads/{context.Policy.BaseBranch}",
            cancellation_token);
        if (state.CurrentBranch is not null)
        {
            await ValidateDirectRefAsync(
                state.WorktreeRoot,
                $"refs/heads/{state.CurrentBranch}",
                cancellation_token);
        }

        var snapshot = await DiscoverStatusSnapshotAsync(state.WorktreeRoot, cancellation_token);
        if (!string.Equals(snapshot.Fingerprint, state.MutationFingerprint, StringComparison.Ordinal))
        {
            throw new RepositoryStateException(
                "The index or working tree changed after policy evaluation; no mutation was executed.");
        }
    }

    public async Task RevalidateDirectBranchAsync(
        string worktree_root,
        string branch,
        CancellationToken cancellation_token = default)
    {
        await ValidateBranchNameAsync(worktree_root, branch, cancellation_token);
        await ValidateDirectRefAsync(worktree_root, $"refs/heads/{branch}", cancellation_token);
    }

    public async Task RevalidateWorktreeAsync(
        RepositoryContext context,
        Worktree worktree,
        string expected_branch,
        string expected_commit,
        CancellationToken cancellation_token = default)
    {
        try
        {
            var root = await DiscoverPathAsync(worktree.Path, "--show-toplevel", cancellation_token);
            var git_directory = await DiscoverPathAsync(worktree.Path, "--git-dir", cancellation_token);
            var common_git_directory = await DiscoverPathAsync(worktree.Path, "--git-common-dir", cancellation_token);
            if (!PathsEqual(root, worktree.Path) || IsReparseDirectory(root) ||
                !PathsEqual(common_git_directory, context.State.CommonGitDirectory))
            {
                throw new RepositoryStateException(
                    $"Worktree '{worktree.Path}' no longer belongs to the evaluated repository.");
            }

            await ValidateRepositoryConfigurationAsync(
                root,
                git_directory,
                common_git_directory,
                cancellation_token);
            var branch = await git.RequireTextAsync(
                root,
                ["symbolic-ref", "--quiet", "--short", "--no-recurse", "HEAD"],
                cancellation_token);
            var head = await git.RequireTextAsync(
                root,
                ["rev-parse", "--verify", "HEAD^{commit}"],
                cancellation_token);
            if (!string.Equals(branch, expected_branch, StringComparison.Ordinal) ||
                !string.Equals(head, expected_commit, StringComparison.Ordinal))
            {
                throw new RepositoryStateException(
                    $"Worktree '{worktree.Path}' changed branch or HEAD after policy evaluation.");
            }
        }
        catch (GitCommandException exception)
        {
            throw new RepositoryStateException(
                $"Unable to revalidate worktree '{worktree.Path}' immediately before mutation.",
                exception);
        }
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
        await ValidateDirectRefAsync(context.State.WorktreeRoot, reference, cancellation_token);

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
            ["check-ref-format", "--branch", branch],
            cancellation_token: cancellation_token);
        var normalized = result.ExitCode == 0
            ? Encoding.UTF8.GetString(result.StandardOutput).TrimEnd('\r', '\n', '\0')
            : null;
        if (result.ExitCode != 0 || !string.Equals(normalized, branch, StringComparison.Ordinal))
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

    public async Task<IReadOnlyList<string>> FindUnsafeCheckoutPathsAsync(
        RepositoryContext context,
        string target_commit,
        CancellationToken cancellation_token = default)
    {
        var root = context.State.WorktreeRoot;
        var target_result = await git.RunAsync(
            root,
            ["ls-tree", "-r", "-z", "--name-only", target_commit],
            cancellation_token: cancellation_token);
        GitClient.EnsureSuccess(target_result, ["ls-tree"]);

        var current_result = await git.RunAsync(
            root,
            ["ls-files", "-z"],
            cancellation_token: cancellation_token);
        GitClient.EnsureSuccess(current_result, ["ls-files"]);

        var comparer = OperatingSystem.IsWindows() ? StringComparer.OrdinalIgnoreCase : StringComparer.Ordinal;
        var current_paths = Encoding.UTF8.GetString(current_result.StandardOutput)
            .Split('\0', StringSplitOptions.RemoveEmptyEntries)
            .ToHashSet(comparer);
        var unsafe_paths = new List<string>();
        foreach (var target_path in Encoding.UTF8.GetString(target_result.StandardOutput)
                     .Split('\0', StringSplitOptions.RemoveEmptyEntries))
        {
            var native_path = target_path.Replace('/', Path.DirectorySeparatorChar);
            var absolute_path = Path.GetFullPath(native_path, root);
            if (!IsWithin(root, absolute_path))
            {
                throw new RepositoryException($"Git tree contains a path outside the worktree: '{target_path}'.");
            }

            if (IsReparsePoint(absolute_path) || HasReparseDirectoryAncestor(root, absolute_path) ||
                !current_paths.Contains(target_path) &&
                (File.Exists(absolute_path) || Directory.Exists(absolute_path)))
            {
                unsafe_paths.Add(target_path);
                if (unsafe_paths.Count == 4)
                {
                    break;
                }
            }
        }

        return unsafe_paths;
    }

    public async Task<IReadOnlyList<string>> FindUnsafeAddPathsAsync(
        RepositoryContext context,
        IReadOnlyList<string> relative_paths,
        CancellationToken cancellation_token = default)
    {
        var root = context.State.WorktreeRoot;
        var arguments = new List<string> { "ls-files", "--cached", "--others", "--exclude-standard", "-z", "--" };
        arguments.AddRange(relative_paths);
        var result = await git.RunAsync(root, arguments, cancellation_token: cancellation_token);
        GitClient.EnsureSuccess(result, ["ls-files"]);

        var candidates = Encoding.UTF8.GetString(result.StandardOutput)
            .Split('\0', StringSplitOptions.RemoveEmptyEntries)
            .Concat(relative_paths)
            .Distinct(OperatingSystem.IsWindows() ? StringComparer.OrdinalIgnoreCase : StringComparer.Ordinal);
        var unsafe_paths = new List<string>();
        foreach (var candidate in candidates)
        {
            var native_path = candidate.Replace('/', Path.DirectorySeparatorChar);
            var absolute_path = Path.GetFullPath(native_path, root);
            if (!IsWithin(root, absolute_path))
            {
                throw new RepositoryException($"Git selected a path outside the worktree: '{candidate}'.");
            }

            if (IsReparsePoint(absolute_path) || HasReparseDirectoryAncestor(root, absolute_path))
            {
                unsafe_paths.Add(candidate);
                if (unsafe_paths.Count == 4)
                {
                    break;
                }
            }
        }

        return unsafe_paths;
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
        string git_directory,
        string common_git_directory,
        CancellationToken cancellation_token)
    {
        if (!IsWithin(common_git_directory, git_directory) ||
            IsReparseDirectory(git_directory) ||
            HasReparseDirectoryAncestor(common_git_directory, git_directory))
        {
            throw new RepositoryStateException(
                $"Git directory '{git_directory}' is outside or redirected from the registered common Git directory.");
        }

        ValidateAdministrativeLayout(worktree_root, git_directory, common_git_directory);
        var common_config_path = Path.Combine(common_git_directory, "config");
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
            var worktree_config_path = await git.RequireTextAsync(
                worktree_root,
                ["rev-parse", "--path-format=absolute", "--git-path", "config.worktree"],
                cancellation_token);
            if (Directory.Exists(worktree_config_path))
            {
                throw new RepositoryStateException(
                    $"Worktree Git configuration path is a directory: '{worktree_config_path}'.");
            }

            if (File.Exists(worktree_config_path))
            {
                if (!IsWithin(git_directory, worktree_config_path) || IsReparsePoint(worktree_config_path))
                {
                    throw new RepositoryStateException(
                        $"Worktree Git configuration is outside or redirected from its Git directory: " +
                        $"'{worktree_config_path}'.");
                }

                scopes.Add("--worktree");
            }
        }
        else if (worktree_config.ExitCode is not (0 or 1))
        {
            GitClient.EnsureSuccess(worktree_config, ["config"]);
        }

        foreach (var scope in scopes)
        {
            var result = await git.RunAsync(
                worktree_root,
                ["config", scope, "--null", "--name-only", "--list"],
                cancellation_token: cancellation_token);
            GitClient.EnsureSuccess(result, ["config"]);
            var names = GitOutputParsers.ParseConfigNames(result.StandardOutput);
            var unknown = names.Where(name =>
                IsUnsupportedExecutableConfiguration(name, git.HasTrustedGitLfs)).ToArray();
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

        var graft_directories = new[] { git_directory, common_git_directory }.Distinct(
            OperatingSystem.IsWindows() ? StringComparer.OrdinalIgnoreCase : StringComparer.Ordinal);
        if (graft_directories.Any(directory => File.Exists(Path.Combine(directory, "info", "grafts"))))
        {
            throw new RepositoryStateException("Git graft files are not supported by agent-git.");
        }
    }

    private static void ValidateAdministrativeLayout(
        string worktree_root,
        string git_directory,
        string common_git_directory)
    {
        foreach (var path in new[]
                 {
                     Path.Combine(worktree_root, ".git"),
                     Path.Combine(common_git_directory, "config"),
                     Path.Combine(common_git_directory, "packed-refs"),
                     Path.Combine(common_git_directory, "shallow"),
                     Path.Combine(git_directory, "HEAD"),
                     Path.Combine(git_directory, "index"),
                     Path.Combine(git_directory, "commondir"),
                     Path.Combine(git_directory, "gitdir"),
                 })
        {
            if (IsReparsePoint(path))
            {
                throw new RepositoryStateException(
                    $"Git administrative path cannot be a symbolic link or reparse point: '{path}'.");
            }
        }

        ValidateAdministrativeTree(Path.Combine(common_git_directory, "refs"), include_files: true);
        ValidateAdministrativeTree(Path.Combine(common_git_directory, "logs"), include_files: true);
        ValidateAdministrativeTree(Path.Combine(common_git_directory, "reftable"), include_files: true);
        ValidateAdministrativeTree(Path.Combine(common_git_directory, "worktrees"), include_files: true);
        ValidateAdministrativeTree(Path.Combine(common_git_directory, "objects"), include_files: false);
    }

    private static void ValidateAdministrativeTree(string root, bool include_files)
    {
        if (!Directory.Exists(root))
        {
            return;
        }

        var pending = new Stack<string>();
        pending.Push(root);
        while (pending.TryPop(out var directory))
        {
            foreach (var entry in Directory.EnumerateFileSystemEntries(directory))
            {
                var attributes = File.GetAttributes(entry);
                var is_directory = (attributes & FileAttributes.Directory) != 0;
                if ((attributes & FileAttributes.ReparsePoint) != 0)
                {
                    if (is_directory || include_files)
                    {
                        throw new RepositoryStateException(
                            $"Git administrative path cannot be a symbolic link or reparse point: '{entry}'.");
                    }

                    continue;
                }

                if (is_directory)
                {
                    pending.Push(entry);
                }
            }
        }
    }

    private static bool IsUnsupportedExecutableConfiguration(string name, bool has_trusted_git_lfs)
    {
        var lower = name.ToLowerInvariant();
        if (lower.StartsWith("include.", StringComparison.Ordinal) ||
            lower.StartsWith("includeif.", StringComparison.Ordinal))
        {
            return true;
        }

        if (lower.StartsWith("merge.", StringComparison.Ordinal) && lower.EndsWith(".driver", StringComparison.Ordinal))
        {
            return true;
        }

        if (lower.StartsWith("credential.", StringComparison.Ordinal) ||
            lower.StartsWith("alias.", StringComparison.Ordinal) ||
            lower.StartsWith("difftool.", StringComparison.Ordinal) ||
            lower.StartsWith("mergetool.", StringComparison.Ordinal) ||
            lower.StartsWith("pager.", StringComparison.Ordinal) ||
            lower.Equals("core.alternaterefscommand", StringComparison.Ordinal) ||
            lower.Equals("core.askpass", StringComparison.Ordinal) ||
            lower.Equals("core.excludesfile", StringComparison.Ordinal) ||
            lower.Equals("core.fsmonitor", StringComparison.Ordinal) ||
            lower.Equals("core.gitproxy", StringComparison.Ordinal) ||
            lower.Equals("core.pager", StringComparison.Ordinal) ||
            lower.Equals("core.sshcommand", StringComparison.Ordinal) ||
            lower.Equals("core.worktree", StringComparison.Ordinal) ||
            lower.Equals("diff.external", StringComparison.Ordinal) ||
            lower.Equals("extensions.partialclone", StringComparison.Ordinal) ||
            (lower.StartsWith("gpg.", StringComparison.Ordinal) &&
             lower.EndsWith(".program", StringComparison.Ordinal)) ||
            lower.Equals("gpg.program", StringComparison.Ordinal) ||
            lower.Equals("interactive.difffilter", StringComparison.Ordinal) ||
            lower.StartsWith("lfs.customtransfer.", StringComparison.Ordinal) ||
            lower.StartsWith("lfs.extension.", StringComparison.Ordinal) ||
            lower.Equals("lfs.storage", StringComparison.Ordinal) ||
            lower.Equals("lfs.standalonetransferagent", StringComparison.Ordinal) ||
            (lower.StartsWith("remote.", StringComparison.Ordinal) &&
             (lower.EndsWith(".promisor", StringComparison.Ordinal) ||
              lower.EndsWith(".partialclonefilter", StringComparison.Ordinal))) ||
            lower.StartsWith("submodule.", StringComparison.Ordinal) && lower.EndsWith(".update", StringComparison.Ordinal))
        {
            return true;
        }

        if (lower.StartsWith("diff.", StringComparison.Ordinal) &&
            (lower.EndsWith(".command", StringComparison.Ordinal) ||
             lower.EndsWith(".textconv", StringComparison.Ordinal)))
        {
            return true;
        }

        if (!lower.StartsWith("filter.", StringComparison.Ordinal) ||
            !(lower.EndsWith(".clean", StringComparison.Ordinal) ||
              lower.EndsWith(".smudge", StringComparison.Ordinal) ||
              lower.EndsWith(".process", StringComparison.Ordinal)))
        {
            return false;
        }

        return !has_trusted_git_lfs || !lower.StartsWith("filter.lfs.", StringComparison.Ordinal);
    }

    private async Task ValidateIndexFlagsAsync(
        string worktree_root,
        CancellationToken cancellation_token)
    {
        var result = await git.RunAsync(
            worktree_root,
            ["ls-files", "-v", "-z"],
            cancellation_token: cancellation_token);
        GitClient.EnsureSuccess(result, ["ls-files"]);

        var unsupported = Encoding.UTF8.GetString(result.StandardOutput)
            .Split('\0', StringSplitOptions.RemoveEmptyEntries)
            .Where(record => record.Length < 3 || record[1] != ' ' ||
                record[0] == 'S' || char.IsLower(record[0]))
            .Take(4)
            .ToArray();
        if (unsupported.Length > 0)
        {
            var paths = unsupported.Select(record => record.Length >= 3 ? record[2..] : "<malformed>");
            throw new RepositoryStateException(
                "Assume-unchanged and skip-worktree index entries are not supported because they can hide " +
                $"working-tree changes: {string.Join(", ", paths)}.");
        }
    }

    private async Task ValidateDirectRefAsync(
        string worktree_root,
        string reference,
        CancellationToken cancellation_token)
    {
        var symref = await git.RunAsync(
            worktree_root,
            ["symbolic-ref", "--quiet", reference],
            cancellation_token: cancellation_token);
        if (symref.ExitCode == 0)
        {
            throw new RepositoryStateException(
                $"Branch ref '{reference}' is symbolic and is not safe for agent-git.");
        }

        if (symref.ExitCode != 1)
        {
            GitClient.EnsureSuccess(symref, ["symbolic-ref"]);
        }
    }

    private async Task<StatusSnapshot> DiscoverStatusSnapshotAsync(
        string worktree_root,
        CancellationToken cancellation_token)
    {
        var status_result = await git.RunAsync(
            worktree_root,
            ["status", "--porcelain=v2", "-z", "--untracked-files=all", "--ignore-submodules=none"],
            cancellation_token: cancellation_token);
        GitClient.EnsureSuccess(status_result, ["status"]);
        var index_result = await git.RunAsync(
            worktree_root,
            ["ls-files", "--stage", "-z"],
            cancellation_token: cancellation_token);
        GitClient.EnsureSuccess(index_result, ["ls-files"]);

        var status = GitOutputParsers.ParseStatus(status_result.StandardOutput);

        var fingerprint = $"{Convert.ToHexString(SHA256.HashData(status_result.StandardOutput))}:" +
            Convert.ToHexString(SHA256.HashData(index_result.StandardOutput));
        return new StatusSnapshot(
            status,
            fingerprint);
    }

    private sealed record StatusSnapshot(WorkingTreeStatus Status, string Fingerprint);

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

    private static bool HasReparseDirectoryAncestor(string root, string path)
    {
        var directory = Path.GetDirectoryName(path);
        while (directory is not null && IsWithin(root, directory) && !PathsEqual(root, directory))
        {
            if (IsReparseDirectory(directory))
            {
                return true;
            }

            directory = Path.GetDirectoryName(directory);
        }

        return false;
    }

    private static bool IsReparseDirectory(string path)
    {
        return Directory.Exists(path) && IsReparsePoint(path);
    }

    private static bool IsReparsePoint(string path)
    {
        try
        {
            return (File.GetAttributes(path) & FileAttributes.ReparsePoint) != 0;
        }
        catch (Exception exception) when (exception is FileNotFoundException or DirectoryNotFoundException)
        {
            return false;
        }
    }

    private static bool PathsEqual(string left, string right)
    {
        return string.Equals(
            Path.GetFullPath(left).TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar),
            Path.GetFullPath(right).TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar),
            OperatingSystem.IsWindows() ? StringComparison.OrdinalIgnoreCase : StringComparison.Ordinal);
    }
}
