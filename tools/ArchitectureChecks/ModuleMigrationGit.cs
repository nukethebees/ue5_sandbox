using System.Text;

namespace ArchitectureChecks;

internal sealed record RenamedFile(string SourcePath, string DestinationPath, string DestinationModule);

internal sealed class ModuleMigrationGit(IProcessRunner process_runner)
{
    private static readonly TimeSpan query_timeout = TimeSpan.FromSeconds(30);

    public async Task<string> GetRepositoryRootAsync(string root_path, CancellationToken cancellation_token)
    {
        var root = Path.GetFullPath(root_path);
        if (!Directory.Exists(root))
        {
            throw new DirectoryNotFoundException($"Repository root does not exist: '{root}'.");
        }

        var result = await RunGitAsync(root, ["rev-parse", "--show-toplevel"], cancellation_token);
        var repository_root = Encoding.UTF8.GetString(result.StandardOutput).TrimEnd('\r', '\n');
        if (string.IsNullOrWhiteSpace(repository_root))
        {
            throw new ModuleMigrationException("Git repository discovery produced an empty repository root.");
        }

        return Path.GetFullPath(repository_root);
    }

    public async Task<IReadOnlyList<RenamedFile>> GetRenamesAsync(
        string repository_root,
        ModuleMigrationRequest request,
        CancellationToken cancellation_token)
    {
        var arguments = new List<string>
        {
            "-c", "core.safecrlf=false",
            "diff", "--name-status", "-z", "--find-renames=20%", request.Baseline, "--",
            $"Source/{request.OldModule}",
        };
        arguments.AddRange(request.PluginModules.Select(module => $"Plugins/{module}"));

        var result = await RunGitAsync(repository_root, arguments, cancellation_token);
        return ParseRenameRecords(result.StandardOutput, request.OldModule, request.PluginModules);
    }

    public async Task<string> ReadBaselineFileAsync(
        string repository_root,
        string baseline,
        string relative_path,
        CancellationToken cancellation_token)
    {
        var result = await RunGitAsync(repository_root, ["show", $"{baseline}:{relative_path}"], cancellation_token);
        return Encoding.UTF8.GetString(result.StandardOutput);
    }

    internal static IReadOnlyList<RenamedFile> ParseRenameRecords(
        byte[] output,
        string old_module,
        IReadOnlyList<string> plugin_modules)
    {
        ArgumentNullException.ThrowIfNull(output);
        ArgumentException.ThrowIfNullOrWhiteSpace(old_module);
        ArgumentNullException.ThrowIfNull(plugin_modules);

        var fields = ParseNullDelimitedFields(output);
        var renames = new List<RenamedFile>();
        for (var index = 0; index < fields.Count;)
        {
            var status = fields[index++];
            if (string.IsNullOrEmpty(status))
            {
                throw new ModuleMigrationException("Git diff emitted an empty status record.");
            }

            var requires_two_paths = status.StartsWith('R') || status.StartsWith('C');
            var required_paths = requires_two_paths ? 2 : 1;
            if (fields.Count - index < required_paths)
            {
                throw new ModuleMigrationException($"Git diff emitted an incomplete '{status}' record.");
            }

            var first_path = fields[index++];
            var second_path = requires_two_paths ? fields[index++] : null;
            if (!status.StartsWith('R') || second_path is null || !IsPathInsideSourceModule(first_path, old_module))
            {
                continue;
            }

            var destination_module = FindDestinationModule(second_path, plugin_modules);
            if (destination_module is not null)
            {
                renames.Add(new RenamedFile(first_path, second_path, destination_module));
            }
        }

        return renames;
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
                new ProcessRequest("git", git_arguments, Path.GetFullPath(repository_root), query_timeout),
                cancellation_token);
            if (result.ExitCode == 0)
            {
                return result;
            }

            var diagnostic = string.IsNullOrWhiteSpace(result.StandardError)
                ? $"Git exited with code {result.ExitCode}."
                : result.StandardError.Trim();
            throw new ModuleMigrationException($"Git command failed: {diagnostic}");
        }
        catch (ProcessLaunchException exception)
        {
            throw new ModuleMigrationException("Unable to start Git. Ensure 'git' is installed and available on PATH.", exception);
        }
        catch (ProcessTimeoutException exception)
        {
            throw new ModuleMigrationException($"Git command timed out: {exception.Message}", exception);
        }
    }

    private static IReadOnlyList<string> ParseNullDelimitedFields(byte[] output)
    {
        var fields = new List<string>();
        var start = 0;
        for (var index = 0; index < output.Length; ++index)
        {
            if (output[index] != 0)
            {
                continue;
            }

            fields.Add(Encoding.UTF8.GetString(output, start, index - start));
            start = index + 1;
        }

        if (start != output.Length)
        {
            throw new ModuleMigrationException("Git diff output was not NUL terminated.");
        }

        return fields;
    }

    private static bool IsPathInsideSourceModule(string path, string old_module)
    {
        return path.StartsWith($"Source/{old_module}/", StringComparison.Ordinal);
    }

    private static string? FindDestinationModule(string path, IReadOnlyList<string> plugin_modules)
    {
        foreach (var module in plugin_modules)
        {
            if (path.StartsWith($"Plugins/{module}/Source/{module}/", StringComparison.Ordinal))
            {
                return module;
            }
        }

        return null;
    }
}
