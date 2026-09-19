using System.Security.Cryptography;
using System.Text;
using System.Text.Json;
using System.Text.Json.Serialization;

namespace AgentGit;

internal sealed class RebaseRecoveryStore(GitClient git)
{
    internal const string MarkerFileName = "agent-git-rebase-recovery.json";
    internal const string SentinelFileName = "agent-git-owned";

    private const int marker_version = 1;
    private const int maximum_control_file_bytes = 1024 * 1024;
    private const string prepared_phase = "prepared";
    private const string active_phase = "active";

    private static readonly JsonSerializerOptions json_options = new()
    {
        PropertyNamingPolicy = JsonNamingPolicy.CamelCase,
        PropertyNameCaseInsensitive = false,
        UnmappedMemberHandling = JsonUnmappedMemberHandling.Disallow,
        WriteIndented = true,
    };

    public async Task<RebaseRecoveryState> DiscoverAsync(
        TrustedRepository registration,
        GitPolicy policy,
        string worktree_root,
        string git_directory,
        string head_commit,
        string? current_branch,
        RepositoryOperationState operation_state,
        CancellationToken cancellation_token = default)
    {
        var marker_path = MarkerPath(git_directory);
        if (operation_state != RepositoryOperationState.Rebase)
        {
            return File.Exists(marker_path)
                ? new RebaseRecoveryState(
                    RebaseRecoveryAvailability.Stale,
                    null,
                    null,
                    "stale recovery metadata is present without an active rebase")
                : new RebaseRecoveryState(RebaseRecoveryAvailability.None, null, null, "none");
        }

        var rebase_directory = RebaseDirectory(git_directory);
        if (!Directory.Exists(rebase_directory))
        {
            return Unavailable("the active rebase does not use AgentGit's required merge backend");
        }

        ValidateAdministrativePath(git_directory, rebase_directory, expect_directory: true);
        if (!TryReadMarker(git_directory, out var marker, out var marker_json, out var marker_error))
        {
            return Unavailable(marker_error);
        }

        var active_marker = marker!;
        if (!HasValidCoreFields(active_marker) ||
            !string.Equals(active_marker.Phase, active_phase, StringComparison.Ordinal) ||
            !IsToken(active_marker.PlanHash))
        {
            return Unavailable("the AgentGit recovery marker is not active or has invalid fields");
        }

        var sentinel_path = SentinelPath(git_directory);
        if (!TryReadControlFile(git_directory, sentinel_path, out var sentinel, out var sentinel_error) ||
            !string.Equals(sentinel, active_marker.Token, StringComparison.Ordinal))
        {
            return Unavailable(sentinel_error ?? "the AgentGit recovery sentinel does not match the marker");
        }

        if (!string.Equals(active_marker.RepositoryId, registration.RepositoryId, StringComparison.Ordinal) ||
            !string.Equals(active_marker.BaseBranch, policy.BaseBranch, StringComparison.Ordinal))
        {
            return Unavailable("the AgentGit recovery marker does not match this trusted repository");
        }

        if (!await ValidateGitStateAsync(
                active_marker,
                worktree_root,
                git_directory,
                head_commit,
                current_branch,
                cancellation_token))
        {
            return Unavailable("Git rebase state does not match AgentGit recovery provenance");
        }

        var plan_hash = await ReadAndValidatePlanHashAsync(
            active_marker,
            worktree_root,
            git_directory,
            cancellation_token);
        if (!string.Equals(plan_hash, active_marker.PlanHash, StringComparison.Ordinal))
        {
            return Unavailable("the active rebase plan changed after AgentGit established recovery provenance");
        }

        var fingerprint = ComputeFingerprint(marker_json!, sentinel!, plan_hash);
        var continuation_allowed =
            policy.Classify(active_marker.OriginalBranch) == BranchClassification.Feature &&
            policy.Operations.TryGetValue(AgentGitOperation.RebaseBase, out var rebase_policy) &&
            rebase_policy.AllowedCurrentGroups.Contains(BranchClassification.Feature);
        return continuation_allowed
            ? new RebaseRecoveryState(
                RebaseRecoveryAvailability.Available,
                active_marker,
                fingerprint,
                "AgentGit-owned rebase recovery is available")
            : new RebaseRecoveryState(
                RebaseRecoveryAvailability.AbortOnly,
                active_marker,
                fingerprint,
                "current policy forbids continuation; only abort remains available");
    }

    public RebaseRecoveryMarker Prepare(RepositoryContext context)
    {
        if (context.State.OperationState != RepositoryOperationState.None ||
            context.State.CurrentBranch is null)
        {
            throw new RepositoryStateException("Cannot prepare rebase recovery outside a normal branch state.");
        }

        var marker_path = MarkerPath(context.State.GitDirectory);
        ValidateAdministrativePath(context.State.GitDirectory, marker_path, expect_directory: false);
        if (File.Exists(marker_path))
        {
            File.Delete(marker_path);
        }

        var marker = new RebaseRecoveryMarker(
            marker_version,
            prepared_phase,
            Convert.ToHexString(RandomNumberGenerator.GetBytes(32)),
            context.Registration.RepositoryId,
            context.State.CurrentBranch,
            context.State.HeadCommit,
            context.Policy.BaseBranch,
            context.State.BaseCommit,
            context.State.PolicyCommit,
            null);
        WriteMarker(marker_path, marker, FileMode.CreateNew);
        return marker;
    }

    public async Task PromoteAsync(
        RepositoryContext start_context,
        RepositoryContext active_context,
        CancellationToken cancellation_token = default)
    {
        if (active_context.State.OperationState != RepositoryOperationState.Rebase ||
            active_context.State.CurrentBranch is not null ||
            start_context.State.CurrentBranch is null ||
            !string.Equals(
                start_context.State.GitDirectory,
                active_context.State.GitDirectory,
                OperatingSystem.IsWindows() ? StringComparison.OrdinalIgnoreCase : StringComparison.Ordinal))
        {
            throw new RepositoryStateException(
                "Git failed without establishing the expected detached rebase state; recovery was not enabled.");
        }

        var marker_path = MarkerPath(active_context.State.GitDirectory);
        if (!TryReadMarker(active_context.State.GitDirectory, out var prepared, out _, out var marker_error))
        {
            throw new RepositoryStateException(
                $"Prepared rebase recovery provenance changed before activation: {marker_error}.");
        }

        var prepared_marker = prepared!;
        if (!HasValidCoreFields(prepared_marker) ||
            !string.Equals(prepared_marker.Phase, prepared_phase, StringComparison.Ordinal) ||
            prepared_marker.PlanHash is not null ||
            !string.Equals(prepared_marker.RepositoryId, start_context.Registration.RepositoryId, StringComparison.Ordinal) ||
            !string.Equals(prepared_marker.OriginalBranch, start_context.State.CurrentBranch, StringComparison.Ordinal) ||
            !string.Equals(prepared_marker.OriginalHead, start_context.State.HeadCommit, StringComparison.OrdinalIgnoreCase) ||
            !string.Equals(prepared_marker.BaseBranch, start_context.Policy.BaseBranch, StringComparison.Ordinal) ||
            !string.Equals(prepared_marker.BaseCommit, start_context.State.BaseCommit, StringComparison.OrdinalIgnoreCase) ||
            !string.Equals(prepared_marker.PolicyCommit, start_context.State.PolicyCommit, StringComparison.OrdinalIgnoreCase))
        {
            throw new RepositoryStateException(
                "Prepared rebase recovery provenance no longer matches the authorized rebase start.");
        }

        if (!await ValidateGitStateAsync(
                prepared_marker,
                active_context.State.WorktreeRoot,
                active_context.State.GitDirectory,
                active_context.State.HeadCommit,
                active_context.State.CurrentBranch,
                cancellation_token))
        {
            throw new RepositoryStateException(
                "Git's active rebase metadata does not match the rebase AgentGit attempted to start.");
        }

        var plan_hash = await ReadAndValidatePlanHashAsync(
            prepared_marker,
            active_context.State.WorktreeRoot,
            active_context.State.GitDirectory,
            cancellation_token);
        var sentinel_path = SentinelPath(active_context.State.GitDirectory);
        ValidateAdministrativePath(active_context.State.GitDirectory, sentinel_path, expect_directory: false);
        WriteControlFile(sentinel_path, prepared_marker.Token, FileMode.CreateNew);

        var active = prepared_marker with { Phase = active_phase, PlanHash = plan_hash };
        WriteMarker(marker_path, active, FileMode.Create);
    }

    public void RemoveMarker(string git_directory)
    {
        var marker_path = MarkerPath(git_directory);
        ValidateAdministrativePath(git_directory, marker_path, expect_directory: false);
        if (File.Exists(marker_path))
        {
            File.Delete(marker_path);
        }
    }

    private async Task<bool> ValidateGitStateAsync(
        RebaseRecoveryMarker marker,
        string worktree_root,
        string git_directory,
        string head_commit,
        string? current_branch,
        CancellationToken cancellation_token)
    {
        if (current_branch is not null ||
            !IsObjectId(marker.OriginalHead) ||
            !IsObjectId(marker.BaseCommit) ||
            !IsObjectId(marker.PolicyCommit))
        {
            return false;
        }

        var head_name_path = Path.Combine(RebaseDirectory(git_directory), "head-name");
        var original_head_path = Path.Combine(RebaseDirectory(git_directory), "orig-head");
        var onto_path = Path.Combine(RebaseDirectory(git_directory), "onto");
        if (!TryReadControlFile(git_directory, head_name_path, out var head_name, out _) ||
            !TryReadControlFile(git_directory, original_head_path, out var original_head, out _) ||
            !TryReadControlFile(git_directory, onto_path, out var onto, out _) ||
            !string.Equals(head_name, $"refs/heads/{marker.OriginalBranch}", StringComparison.Ordinal) ||
            !string.Equals(original_head, marker.OriginalHead, StringComparison.OrdinalIgnoreCase) ||
            !string.Equals(onto, marker.BaseCommit, StringComparison.OrdinalIgnoreCase))
        {
            return false;
        }

        var branch_result = await git.RunAsync(
            worktree_root,
            ["check-ref-format", "--branch", marker.OriginalBranch],
            cancellation_token: cancellation_token);
        if (branch_result.ExitCode != 0)
        {
            return false;
        }

        var symbolic_branch = await git.RunAsync(
            worktree_root,
            ["symbolic-ref", "--quiet", $"refs/heads/{marker.OriginalBranch}"],
            cancellation_token: cancellation_token);
        if (symbolic_branch.ExitCode != 1)
        {
            return false;
        }

        var branch_commit = await git.RequireTextAsync(
            worktree_root,
            ["rev-parse", "--verify", $"refs/heads/{marker.OriginalBranch}^{{commit}}"],
            cancellation_token);
        if (!string.Equals(branch_commit, marker.OriginalHead, StringComparison.OrdinalIgnoreCase))
        {
            return false;
        }

        var head_descends_from_base = await git.RunAsync(
            worktree_root,
            ["merge-base", "--is-ancestor", marker.BaseCommit, head_commit],
            cancellation_token: cancellation_token);
        return head_descends_from_base.ExitCode == 0;
    }

    private async Task<string> ReadAndValidatePlanHashAsync(
        RebaseRecoveryMarker marker,
        string worktree_root,
        string git_directory,
        CancellationToken cancellation_token)
    {
        var commands = new List<string>();
        foreach (var file_name in new[] { "done", "git-rebase-todo" })
        {
            var path = Path.Combine(RebaseDirectory(git_directory), file_name);
            if (!File.Exists(path))
            {
                continue;
            }

            ValidateAdministrativePath(git_directory, path, expect_directory: false);
            var info = new FileInfo(path);
            if (info.Length > maximum_control_file_bytes)
            {
                throw new RepositoryStateException($"Git rebase control file is too large: '{path}'.");
            }

            foreach (var raw_line in File.ReadAllLines(path))
            {
                var line = raw_line.Trim();
                if (line.Length == 0 || line[0] == '#')
                {
                    continue;
                }

                var separator = line.IndexOf(' ');
                if (separator <= 0 || !string.Equals(line[..separator], "pick", StringComparison.Ordinal))
                {
                    throw new RepositoryStateException(
                        "AgentGit recovery permits only the non-interactive pick sequence it started.");
                }

                var remainder = line[(separator + 1)..].TrimStart();
                var oid_end = remainder.IndexOf(' ');
                var oid = oid_end < 0 ? remainder : remainder[..oid_end];
                if (!IsObjectId(oid))
                {
                    throw new RepositoryStateException("Git rebase plan contains an invalid commit object ID.");
                }

                var belongs_to_original_branch = await git.RunAsync(
                    worktree_root,
                    ["merge-base", "--is-ancestor", oid, marker.OriginalHead],
                    cancellation_token: cancellation_token);
                if (belongs_to_original_branch.ExitCode != 0)
                {
                    throw new RepositoryStateException(
                        "Git rebase plan contains a commit outside the original feature history.");
                }

                commands.Add(oid.ToLowerInvariant());
            }
        }

        if (commands.Count == 0)
        {
            throw new RepositoryStateException("Git rebase plan contains no replayable commits.");
        }

        return Convert.ToHexString(SHA256.HashData(Encoding.UTF8.GetBytes(string.Join('\n', commands))));
    }

    private static bool TryReadMarker(
        string git_directory,
        out RebaseRecoveryMarker? marker,
        out string? json,
        out string error)
    {
        marker = null;
        json = null;
        error = "the AgentGit recovery marker is missing";
        var marker_path = MarkerPath(git_directory);
        if (!File.Exists(marker_path))
        {
            return false;
        }

        ValidateAdministrativePath(git_directory, marker_path, expect_directory: false);
        if (new FileInfo(marker_path).Length > 16 * 1024)
        {
            error = "the AgentGit recovery marker exceeds its size limit";
            return false;
        }

        try
        {
            json = File.ReadAllText(marker_path, Encoding.UTF8);
            StrictJson.RejectDuplicateProperties(json);
            marker = JsonSerializer.Deserialize<RebaseRecoveryMarker>(json, json_options);
            if (marker is null)
            {
                error = "the AgentGit recovery marker contained null";
                return false;
            }

            return true;
        }
        catch (Exception exception) when (exception is JsonException or IOException or UnauthorizedAccessException)
        {
            error = $"the AgentGit recovery marker is invalid: {exception.Message}";
            return false;
        }
    }

    private static bool TryReadControlFile(
        string git_directory,
        string path,
        out string? value,
        out string? error)
    {
        value = null;
        error = null;
        if (!File.Exists(path))
        {
            error = $"required Git rebase metadata is missing: '{Path.GetFileName(path)}'";
            return false;
        }

        ValidateAdministrativePath(git_directory, path, expect_directory: false);
        var info = new FileInfo(path);
        if (info.Length > maximum_control_file_bytes)
        {
            error = $"Git rebase metadata exceeds its size limit: '{Path.GetFileName(path)}'";
            return false;
        }

        value = File.ReadAllText(path, Encoding.UTF8).TrimEnd('\r', '\n');
        return true;
    }

    private static void WriteMarker(string path, RebaseRecoveryMarker marker, FileMode mode)
    {
        var json = JsonSerializer.Serialize(marker, json_options) + Environment.NewLine;
        WriteBytes(path, Encoding.UTF8.GetBytes(json), mode);
    }

    private static void WriteControlFile(string path, string value, FileMode mode)
    {
        WriteBytes(path, Encoding.UTF8.GetBytes(value + Environment.NewLine), mode);
    }

    private static void WriteBytes(string path, byte[] bytes, FileMode mode)
    {
        using var stream = new FileStream(path, mode, FileAccess.Write, FileShare.None);
        stream.Write(bytes);
        stream.Flush(flushToDisk: true);
    }

    private static string ComputeFingerprint(string marker, string sentinel, string plan_hash)
    {
        var bytes = Encoding.UTF8.GetBytes($"{marker}\0{sentinel}\0{plan_hash}");
        return Convert.ToHexString(SHA256.HashData(bytes));
    }

    private static RebaseRecoveryState Unavailable(string description)
    {
        return new RebaseRecoveryState(
            RebaseRecoveryAvailability.Unavailable,
            null,
            null,
            description);
    }

    private static bool HasValidCoreFields(RebaseRecoveryMarker marker)
    {
        return marker.Version == marker_version &&
            IsToken(marker.Token) &&
            !string.IsNullOrWhiteSpace(marker.RepositoryId) &&
            !string.IsNullOrWhiteSpace(marker.OriginalBranch) &&
            IsObjectId(marker.OriginalHead) &&
            !string.IsNullOrWhiteSpace(marker.BaseBranch) &&
            IsObjectId(marker.BaseCommit) &&
            IsObjectId(marker.PolicyCommit);
    }

    private static bool IsObjectId(string? value)
    {
        return value is { Length: 40 or 64 } && value.All(character =>
            character is >= '0' and <= '9' or >= 'a' and <= 'f' or >= 'A' and <= 'F');
    }

    private static bool IsToken(string? value)
    {
        return value is { Length: 64 } && value.All(character =>
            character is >= '0' and <= '9' or >= 'A' and <= 'F');
    }

    private static string MarkerPath(string git_directory) => Path.Combine(git_directory, MarkerFileName);

    private static string RebaseDirectory(string git_directory) => Path.Combine(git_directory, "rebase-merge");

    private static string SentinelPath(string git_directory) =>
        Path.Combine(RebaseDirectory(git_directory), SentinelFileName);

    private static void ValidateAdministrativePath(string git_directory, string path, bool expect_directory)
    {
        var full_git_directory = Path.GetFullPath(git_directory);
        var full_path = Path.GetFullPath(path);
        var relative = Path.GetRelativePath(full_git_directory, full_path);
        if (Path.IsPathFullyQualified(relative) || relative.Equals("..", StringComparison.Ordinal) ||
            relative.StartsWith($"..{Path.DirectorySeparatorChar}", StringComparison.Ordinal))
        {
            throw new RepositoryStateException(
                $"AgentGit recovery path is outside the current worktree's Git directory: '{full_path}'.");
        }

        var current = full_git_directory;
        foreach (var segment in relative.Split(
                     [Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar],
                     StringSplitOptions.RemoveEmptyEntries))
        {
            current = Path.Combine(current, segment);
            if (File.Exists(current) || Directory.Exists(current))
            {
                if ((File.GetAttributes(current) & FileAttributes.ReparsePoint) != 0)
                {
                    throw new RepositoryStateException(
                        $"AgentGit recovery path cannot be a symbolic link or reparse point: '{current}'.");
                }
            }
        }

        if (expect_directory && File.Exists(full_path))
        {
            throw new RepositoryStateException($"Expected AgentGit recovery directory is a file: '{full_path}'.");
        }

        if (!expect_directory && Directory.Exists(full_path))
        {
            throw new RepositoryStateException($"Expected AgentGit recovery file is a directory: '{full_path}'.");
        }
    }
}
