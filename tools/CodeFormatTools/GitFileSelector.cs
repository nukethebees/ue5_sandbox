using System.Text;

namespace CodeFormatTools;

internal sealed class FormatToolException(string message, Exception? inner_exception = null) : Exception(message, inner_exception);

internal sealed class GitFileSelector(IProcessRunner process_runner)
{
    private static readonly StringComparer path_comparer = OperatingSystem.IsWindows()
        ? StringComparer.OrdinalIgnoreCase
        : StringComparer.Ordinal;

    public async Task<string> GetRepositoryRootAsync(string start_directory, CancellationToken cancellation_token)
    {
        var result = await RunGitAsync(
            start_directory,
            ["rev-parse", "--show-toplevel"],
            cancellation_token);
        var root = Encoding.UTF8.GetString(result.StandardOutput).TrimEnd('\r', '\n');
        if (string.IsNullOrWhiteSpace(root))
        {
            throw new FormatToolException("Git repository discovery produced an empty repository root.");
        }

        return Path.GetFullPath(root);
    }

    public async Task<IReadOnlyList<string>> SelectStagedAsync(
        string repository_root,
        FormattingScope scope,
        CancellationToken cancellation_token)
    {
        var paths = await GetPathsAsync(
            repository_root,
            ["diff", "--cached", "--name-only", "-z", "--diff-filter=ACMR"],
            cancellation_token);
        return FilterAndSort(paths, scope);
    }

    public async Task<IReadOnlyList<string>> SelectChangedAsync(
        string repository_root,
        FormattingScope scope,
        CancellationToken cancellation_token)
    {
        var changed_paths = await GetPathsAsync(
            repository_root,
            ["diff", "--name-only", "-z", "HEAD"],
            cancellation_token);
        var untracked_paths = await GetPathsAsync(
            repository_root,
            ["ls-files", "--others", "--exclude-standard", "-z"],
            cancellation_token);

        return FilterAndSort(changed_paths.Concat(untracked_paths), scope);
    }

    public async Task<IReadOnlyList<string>> SelectUnstagedAsync(
        string repository_root,
        IReadOnlyList<string> selected_files,
        CancellationToken cancellation_token)
    {
        var unstaged_paths = await GetPathsAsync(
            repository_root,
            ["diff", "--name-only", "-z"],
            cancellation_token);
        var selected = new HashSet<string>(selected_files.Select(Path.GetFullPath), path_comparer);
        return unstaged_paths
            .Where(selected.Contains)
            .OrderBy(file_path => file_path, path_comparer)
            .ToArray();
    }

    public async Task StageAsync(
        string repository_root,
        IReadOnlyList<string> files,
        CancellationToken cancellation_token)
    {
        if (files.Count == 0)
        {
            return;
        }

        var arguments = new List<string> { "add", "--" };
        arguments.AddRange(files.Select(file_path => Path.GetRelativePath(repository_root, file_path)));
        await RunGitAsync(repository_root, arguments, cancellation_token);
    }

    internal static IReadOnlyList<string> ParseNullDelimitedPaths(byte[] output, string repository_root)
    {
        ArgumentNullException.ThrowIfNull(output);
        ArgumentException.ThrowIfNullOrWhiteSpace(repository_root);

        var paths = new List<string>();
        var start = 0;
        for (var index = 0; index <= output.Length; index++)
        {
            if (index != output.Length && output[index] != 0)
            {
                continue;
            }

            if (index > start)
            {
                var relative_path = Encoding.UTF8.GetString(output, start, index - start);
                paths.Add(Path.GetFullPath(Path.Combine(repository_root, relative_path)));
            }

            start = index + 1;
        }

        return paths;
    }

    private async Task<IReadOnlyList<string>> GetPathsAsync(
        string repository_root,
        IReadOnlyList<string> arguments,
        CancellationToken cancellation_token)
    {
        var output = await RunGitAsync(repository_root, arguments, cancellation_token);
        return ParseNullDelimitedPaths(output.StandardOutput, repository_root);
    }

    private static IReadOnlyList<string> FilterAndSort(IEnumerable<string> paths, FormattingScope scope)
    {
        return paths
            .Where(scope.IsFormatCandidate)
            .Distinct(path_comparer)
            .OrderBy(file_path => file_path, path_comparer)
            .ToArray();
    }

    private async Task<ProcessResult> RunGitAsync(
        string repository_root,
        IReadOnlyList<string> arguments,
        CancellationToken cancellation_token)
    {
        try
        {
            var git_arguments = new List<string> { "-C", Path.GetFullPath(repository_root) };
            git_arguments.AddRange(arguments);
            var result = await process_runner.RunAsync(
                new ProcessRequest("git", git_arguments, Path.GetFullPath(repository_root), TimeSpan.FromSeconds(30)),
                cancellation_token);
            if (result.ExitCode == 0)
            {
                return result;
            }

            var diagnostic = string.IsNullOrWhiteSpace(result.StandardError)
                ? $"Git exited with code {result.ExitCode}."
                : result.StandardError.Trim();
            throw new FormatToolException($"Git command failed: {diagnostic}");
        }
        catch (ProcessLaunchException exception)
        {
            throw new FormatToolException("Unable to start Git. Ensure 'git' is installed and available on PATH.", exception);
        }
        catch (ProcessTimeoutException exception)
        {
            throw new FormatToolException($"Git command timed out: {exception.Message}", exception);
        }
    }
}

internal sealed record FileSelection(string RepositoryRoot, IReadOnlyList<string> Files, string Description);

internal sealed class FormatFileSelector(
    GitFileSelector git,
    IReadOnlyList<string>? relative_roots = null)
{
    public async Task<FileSelection> SelectAsync(
        FormatMode mode,
        string start_directory,
        Action<string> warning,
        CancellationToken cancellation_token)
    {
        var repository_root = await git.GetRepositoryRootAsync(start_directory, cancellation_token);
        var scope = new FormattingScope(repository_root, relative_roots ?? FormattingScope.DefaultRoots);
        var files = mode switch
        {
            FormatMode.All => scope.SelectAll(warning),
            FormatMode.Changed => await git.SelectChangedAsync(repository_root, scope, cancellation_token),
            FormatMode.Staged => await git.SelectStagedAsync(repository_root, scope, cancellation_token),
            _ => throw new ArgumentOutOfRangeException(nameof(mode), mode, "Unknown format mode."),
        };
        var description = mode switch
        {
            FormatMode.All => "all project source files",
            FormatMode.Changed => "Git-changed files",
            FormatMode.Staged => "staged files",
            _ => throw new ArgumentOutOfRangeException(nameof(mode), mode, "Unknown format mode."),
        };

        return new FileSelection(repository_root, files, description);
    }

    public Task<IReadOnlyList<string>> SelectUnstagedAsync(
        string repository_root,
        IReadOnlyList<string> selected_files,
        CancellationToken cancellation_token)
    {
        return git.SelectUnstagedAsync(repository_root, selected_files, cancellation_token);
    }

    public Task StageAsync(string repository_root, IReadOnlyList<string> files, CancellationToken cancellation_token)
    {
        return git.StageAsync(repository_root, files, cancellation_token);
    }
}
