using System.Text;
using System.Security.Cryptography;
using GitSupport;

namespace AgentGit;

internal sealed class RepositoryDiscovery(GitClient git)
{
    internal RebaseRecoveryStore RebaseRecovery { get; } = new(git);

    public async Task<string> DiscoverCommonGitDirectoryAsync(
        string start_directory,
        CancellationToken cancellation_token = default)
    {
        var working_directory = Path.GetFullPath(start_directory);
        if (!Directory.Exists(working_directory))
        {
            throw new RepositoryException($"Working directory does not exist: '{working_directory}'.");
        }

        return await DiscoverPathAsync(working_directory, "--git-common-dir", cancellation_token);
    }

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

        var paths = await DiscoverPathsAsync(working_directory, cancellation_token);
        var root = paths.WorktreeRoot;
        var git_directory = paths.GitDirectory;
        var common_git_directory = paths.CommonGitDirectory;
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
        var origin_url = await ValidateRepositoryConfigurationAsync(
            working_directory,
            git_directory,
            common_git_directory,
            cancellation_token);
        if (!string.Equals(origin_url, registration.OriginUrl, StringComparison.Ordinal))
        {
            throw new RepositoryException("Repository origin does not match the trusted registration.");
        }

        await ValidateIndexFlagsAsync(root, cancellation_token);
        var local_branches = await DiscoverLocalBranchesAsync(root, cancellation_token);
        if (!local_branches.ContainsKey(registration.PolicyRef))
        {
            throw new PolicyConfigurationException(
                $"Unable to load policy because ref '{registration.PolicyRef}' does not exist.");
        }

        var policy_branch = RequireDirectBranch(local_branches, registration.PolicyRef);
        string policy_commit;
        string policy_json;
        try
        {
            policy_commit = policy_branch.Commit;
            var policy_result = await git.RunAsync(
                root,
                ["cat-file", "blob", $"{registration.PolicyRef}:{registration.PolicyPath}"],
                cancellation_token: cancellation_token,
                maximum_captured_stream_bytes: 64 * 1024);
            GitClient.EnsureSuccess(policy_result, ["cat-file"]);
            policy_json = Encoding.UTF8.GetString(policy_result.StandardOutput).TrimEnd('\r', '\n', '\0');
        }
        catch (ProcessOutputLimitException exception)
        {
            throw new PolicyConfigurationException(
                "Policy document exceeds the 64 KiB safety limit.", exception);
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
            var current_ref = RequireDirectBranch(local_branches, $"refs/heads/{current_branch}");
            if (!string.Equals(current_ref.Commit, head, StringComparison.Ordinal))
            {
                throw new RepositoryStateException("HEAD changed during repository discovery.");
            }
        }
        BranchClassification? classification = current_branch is null ? null : policy.Classify(current_branch);

        var base_commit = policy_commit;
        var status_snapshot = await DiscoverStatusSnapshotAsync(root, cancellation_token);
        var worktrees = await DiscoverWorktreesAsync(root, cancellation_token);
        var current_worktree = worktrees.SingleOrDefault(worktree => PathsEqual(worktree.Path, root));
        if (current_worktree is null)
        {
            throw new RepositoryException($"Git worktree registry does not contain current worktree '{root}'.");
        }

        var home_branch = FindHomeBranch(root, policy, local_branches);
        var operation_state = DiscoverOperationState(git_directory);
        var rebase_recovery = await RebaseRecovery.DiscoverAsync(
            registration,
            policy,
            root,
            git_directory,
            head,
            current_branch,
            operation_state,
            cancellation_token);
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
            rebase_recovery,
            worktrees,
            local_branches,
            home_branch);
        return new RepositoryContext(registration, policy, state);
    }

    public async Task<IReadOnlyDictionary<string, LocalBranchRef>> RevalidateMutationSnapshotAsync(
        RepositoryContext context,
        CancellationToken cancellation_token = default)
    {
        var state = context.State;
        var origin_url = await ValidateRepositoryConfigurationAsync(
            state.WorktreeRoot,
            state.GitDirectory,
            state.CommonGitDirectory,
            cancellation_token);
        if (!string.Equals(origin_url, context.Registration.OriginUrl, StringComparison.Ordinal))
        {
            throw new RepositoryStateException(
                "Repository origin changed after policy evaluation; no mutation was executed.");
        }
        await ValidateIndexFlagsAsync(state.WorktreeRoot, cancellation_token);
        var local_branches = await DiscoverLocalBranchesAsync(state.WorktreeRoot, cancellation_token);
        RequireDirectBranch(local_branches, context.Registration.PolicyRef);
        RequireDirectBranch(local_branches, $"refs/heads/{context.Policy.BaseBranch}");
        if (state.CurrentBranch is not null)
        {
            RequireDirectBranch(local_branches, $"refs/heads/{state.CurrentBranch}");
        }

        var snapshot = await DiscoverStatusSnapshotAsync(state.WorktreeRoot, cancellation_token);
        if (!string.Equals(snapshot.Fingerprint, state.MutationFingerprint, StringComparison.Ordinal))
        {
            throw new RepositoryStateException(
                "The index or working tree changed after policy evaluation; no mutation was executed.");
        }

        if (state.RebaseRecovery.Fingerprint is not null)
        {
            var recovery = await RebaseRecovery.DiscoverAsync(
                context.Registration,
                context.Policy,
                state.WorktreeRoot,
                state.GitDirectory,
                state.HeadCommit,
                state.CurrentBranch,
                state.OperationState,
                cancellation_token);
            if (!string.Equals(
                    recovery.Fingerprint,
                    state.RebaseRecovery.Fingerprint,
                    StringComparison.Ordinal))
            {
                throw new RepositoryStateException(
                    "AgentGit rebase recovery metadata changed after policy evaluation; no mutation was executed.");
            }
        }

        return local_branches;
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
            var paths = await DiscoverPathsAsync(worktree.Path, cancellation_token);
            var root = paths.WorktreeRoot;
            var git_directory = paths.GitDirectory;
            var common_git_directory = paths.CommonGitDirectory;
            if (!PathsEqual(root, worktree.Path) || IsReparseDirectory(root) ||
                !PathsEqual(common_git_directory, context.State.CommonGitDirectory))
            {
                throw new RepositoryStateException(
                    $"Worktree '{worktree.Path}' no longer belongs to the evaluated repository.");
            }

            var origin_url = await ValidateRepositoryConfigurationAsync(
                root,
                git_directory,
                common_git_directory,
                cancellation_token);
            if (!string.Equals(origin_url, context.Registration.OriginUrl, StringComparison.Ordinal))
            {
                throw new RepositoryStateException(
                    $"Worktree '{worktree.Path}' repository origin changed after policy evaluation.");
            }
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
        if (!context.State.LocalBranches.TryGetValue(reference, out var local_branch))
        {
            return null;
        }

        RequireDirectBranch(context.State.LocalBranches, reference);
        var owning_worktree = context.State.Worktrees.SingleOrDefault(worktree =>
            string.Equals(worktree.Branch, branch, StringComparison.OrdinalIgnoreCase));
        return new TargetBranchState(branch, local_branch.Commit, context.Policy.Classify(branch), owning_worktree);
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

    private async Task<RepositoryPaths> DiscoverPathsAsync(
        string working_directory,
        CancellationToken cancellation_token)
    {
        var result = await git.RunAsync(
            working_directory,
            ["rev-parse", "--path-format=absolute", "--show-toplevel", "--git-dir", "--git-common-dir"],
            cancellation_token: cancellation_token);
        if (result.ExitCode != 0)
        {
            var diagnostic = string.IsNullOrWhiteSpace(result.StandardError)
                ? "Git did not identify a repository."
                : result.StandardError.Trim();
            throw new RepositoryException($"Repository discovery failed: {diagnostic}");
        }

        var records = Encoding.UTF8.GetString(result.StandardOutput)
            .Replace("\r\n", "\n", StringComparison.Ordinal)
            .TrimEnd('\n', '\0')
            .Split('\n');
        if (records.Length != 3 || records.Any(string.IsNullOrWhiteSpace))
        {
            throw new RepositoryException("Repository discovery returned malformed path output.");
        }

        return new RepositoryPaths(
            Path.GetFullPath(records[0]),
            Path.GetFullPath(records[1]),
            Path.GetFullPath(records[2]));
    }

    private async Task<string> ValidateRepositoryConfigurationAsync(
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
        var local_result = await git.RunAsync(
            worktree_root,
            ["config", "--local", "--null", "--list"],
            cancellation_token: cancellation_token);
        GitClient.EnsureSuccess(local_result, ["config"]);
        var local_entries = GitOutputParsers.ParseConfigEntries(local_result.StandardOutput);
        ValidateConfigurationNames(local_entries.Select(entry => entry.Name));

        var origin_url = RequireSingleConfigurationValue(local_entries, "remote.origin.url");
        var worktree_config_values = ConfigurationValues(local_entries, "extensions.worktreeConfig");
        if (worktree_config_values.Count > 1)
        {
            throw new RepositoryStateException(
                "Repository Git configuration contains duplicate extensions.worktreeConfig values.");
        }

        var worktree_config_enabled = worktree_config_values.Count == 1 &&
            ParseGitBoolean("extensions.worktreeConfig", worktree_config_values[0]);
        if (worktree_config_enabled)
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

                var worktree_result = await git.RunAsync(
                    worktree_root,
                    ["config", "--worktree", "--null", "--list"],
                    cancellation_token: cancellation_token);
                GitClient.EnsureSuccess(worktree_result, ["config"]);
                var worktree_entries = GitOutputParsers.ParseConfigEntries(worktree_result.StandardOutput);
                ValidateConfigurationNames(worktree_entries.Select(entry => entry.Name));
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

        return origin_url;
    }

    private void ValidateConfigurationNames(IEnumerable<string> names)
    {
        var unknown = names.Where(name =>
            IsUnsupportedExecutableConfiguration(name, git.HasTrustedGitLfs)).ToArray();
        if (unknown.Length > 0)
        {
            throw new RepositoryStateException(
                $"Repository Git configuration contains unsupported executable settings: {string.Join(", ", unknown)}.");
        }
    }

    private static string RequireSingleConfigurationValue(
        IReadOnlyList<GitConfigEntry> entries,
        string name)
    {
        var values = ConfigurationValues(entries, name);
        if (values.Count != 1)
        {
            throw new RepositoryStateException(
                $"Repository Git configuration must contain exactly one {name} value.");
        }

        return values[0];
    }

    private static IReadOnlyList<string> ConfigurationValues(
        IReadOnlyList<GitConfigEntry> entries,
        string name) => entries
        .Where(entry => string.Equals(entry.Name, name, StringComparison.OrdinalIgnoreCase))
        .Select(entry => entry.Value)
        .ToArray();

    private static bool ParseGitBoolean(string name, string value)
    {
        if (value.Length == 0 || value.Equals("true", StringComparison.OrdinalIgnoreCase) ||
            value.Equals("yes", StringComparison.OrdinalIgnoreCase) ||
            value.Equals("on", StringComparison.OrdinalIgnoreCase) || value == "1")
        {
            return true;
        }

        if (value.Equals("false", StringComparison.OrdinalIgnoreCase) ||
            value.Equals("no", StringComparison.OrdinalIgnoreCase) ||
            value.Equals("off", StringComparison.OrdinalIgnoreCase) || value == "0")
        {
            return false;
        }

        throw new RepositoryStateException(
            $"Repository Git configuration contains invalid boolean value '{value}' for {name}.");
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

    private async Task<IReadOnlyDictionary<string, LocalBranchRef>> DiscoverLocalBranchesAsync(
        string worktree_root,
        CancellationToken cancellation_token)
    {
        var result = await git.RunAsync(
            worktree_root,
            [
                "for-each-ref",
                "--format=%(refname)%00%(objectname)%00%(objecttype)%00%(symref)%00%(*objectname)%00%(*objecttype)%00",
                "refs/heads",
            ],
            cancellation_token: cancellation_token);
        GitClient.EnsureSuccess(result, ["for-each-ref"]);

        var branches = GitOutputParsers.ParseLocalBranchRefs(result.StandardOutput);
        try
        {
            return branches.ToDictionary(branch => branch.Reference, StringComparer.Ordinal);
        }
        catch (ArgumentException exception)
        {
            throw new RepositoryStateException(
                "Git for-each-ref returned duplicate local branch records.", exception);
        }
    }

    internal static LocalBranchRef RequireDirectBranch(
        IReadOnlyDictionary<string, LocalBranchRef> local_branches,
        string reference)
    {
        if (!local_branches.TryGetValue(reference, out var branch))
        {
            throw new RepositoryStateException($"Branch ref '{reference}' does not exist.");
        }

        if (branch.IsSymbolic)
        {
            throw new RepositoryStateException(
                $"Branch ref '{reference}' is symbolic and is not safe for agent-git.");
        }

        return branch;
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

    private sealed record RepositoryPaths(
        string WorktreeRoot,
        string GitDirectory,
        string CommonGitDirectory);

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

    private static string? FindHomeBranch(
        string root,
        GitPolicy policy,
        IReadOnlyDictionary<string, LocalBranchRef> local_branches)
    {
        var directory_name = Path.GetFileName(root.TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar));
        if (string.IsNullOrWhiteSpace(directory_name) || policy.Classify(directory_name) != BranchClassification.Workspace)
        {
            return null;
        }

        var reference = $"refs/heads/{directory_name}";
        if (!local_branches.ContainsKey(reference))
        {
            return null;
        }

        RequireDirectBranch(local_branches, reference);
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
