using System.Text;

namespace AgentGit;

internal sealed class GitClient
{
    private static readonly TimeSpan query_timeout = TimeSpan.FromSeconds(30);

    private readonly TrustContext trust;
    private readonly IProcessRunner process_runner;
    private readonly IReadOnlyDictionary<string, string> environment;

    public GitClient(TrustContext trust, IProcessRunner process_runner)
    {
        this.trust = trust;
        this.process_runner = process_runner;
        environment = CreateEnvironment(trust);
    }

    public async Task<ProcessResult> RunAsync(
        string working_directory,
        IReadOnlyList<string> arguments,
        TimeSpan? timeout = null,
        CancellationToken cancellation_token = default)
    {
        var full_arguments = new List<string>
        {
            "--no-pager",
            "--literal-pathspecs",
            "-c", $"core.hooksPath={trust.EmptyHooksDirectory}",
            "-c", "core.fsmonitor=false",
            "-c", "maintenance.auto=false",
            "-c", "gc.auto=0",
            "-c", "commit.gpgSign=false",
            "-c", "tag.gpgSign=false",
            "-c", "rerere.enabled=false",
            "-c", "rerere.autoupdate=false",
            "-c", "submodule.recurse=false",
            "-c", $"user.name={trust.UserName}",
            "-c", $"user.email={trust.UserEmail}",
            "-c", $"core.attributesFile={Path.Combine(trust.InstallationRoot, "config", "empty.attributes")}",
        };
        if (trust.GitLfsExecutable is not null)
        {
            var lfs_command = QuoteFilterExecutable(trust.GitLfsExecutable);
            full_arguments.AddRange(
            [
                "-c", "filter.lfs.clean=",
                "-c", "filter.lfs.smudge=",
                "-c", $"filter.lfs.process={lfs_command} filter-process",
                "-c", "filter.lfs.required=true",
            ]);
        }

        full_arguments.AddRange(arguments);
        return await process_runner.RunAsync(
            new ProcessRequest(
                trust.GitExecutable,
                full_arguments,
                Path.GetFullPath(working_directory),
                environment,
                timeout ?? query_timeout),
            cancellation_token);
    }

    public async Task<string> RequireTextAsync(
        string working_directory,
        IReadOnlyList<string> arguments,
        CancellationToken cancellation_token = default)
    {
        var result = await RunAsync(working_directory, arguments, cancellation_token: cancellation_token);
        EnsureSuccess(result, arguments);
        return Encoding.UTF8.GetString(result.StandardOutput).TrimEnd('\r', '\n', '\0');
    }

    public async Task<ProcessResult> RunMutationAsync(
        string working_directory,
        IReadOnlyList<string> arguments,
        CancellationToken cancellation_token = default)
    {
        return await RunAsync(
            working_directory,
            arguments,
            TimeSpan.FromMinutes(30),
            cancellation_token);
    }

    public static void EnsureSuccess(ProcessResult result, IReadOnlyList<string> arguments)
    {
        if (result.ExitCode == 0)
        {
            return;
        }

        var diagnostic = string.IsNullOrWhiteSpace(result.StandardError)
            ? $"Git exited with code {result.ExitCode} without diagnostic output."
            : result.StandardError.Trim();
        var command = arguments.Count == 0 ? "Git" : $"Git {arguments[0]}";
        throw new GitCommandException($"{command} failed: {diagnostic}", result.ExitCode, result.StandardError);
    }

    private static IReadOnlyDictionary<string, string> CreateEnvironment(TrustContext trust)
    {
        var windows = Environment.GetFolderPath(Environment.SpecialFolder.Windows);
        var user_profile = Environment.GetFolderPath(Environment.SpecialFolder.UserProfile);
        var local_app_data = Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData);
        var path_parts = new[]
        {
            trust.GitLfsExecutable is null ? null : Path.GetDirectoryName(trust.GitLfsExecutable),
            Path.GetDirectoryName(trust.GitExecutable),
            string.IsNullOrWhiteSpace(windows) ? null : Path.Combine(windows, "System32"),
            windows,
        }.Where(value => !string.IsNullOrWhiteSpace(value)).Distinct(StringComparer.OrdinalIgnoreCase);

        return new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase)
        {
            ["SystemRoot"] = windows,
            ["WINDIR"] = windows,
            ["USERPROFILE"] = user_profile,
            ["HOME"] = user_profile,
            ["LOCALAPPDATA"] = local_app_data,
            ["TEMP"] = Path.GetTempPath(),
            ["TMP"] = Path.GetTempPath(),
            ["PATH"] = string.Join(Path.PathSeparator, path_parts),
            ["PATHEXT"] = ".EXE",
            ["LC_ALL"] = "C",
            ["LANG"] = "C",
            ["GIT_CONFIG_NOSYSTEM"] = "1",
            ["GIT_CONFIG_GLOBAL"] = trust.EmptyConfigPath,
            ["GIT_ATTR_NOSYSTEM"] = "1",
            ["GIT_NO_REPLACE_OBJECTS"] = "1",
            ["GIT_TERMINAL_PROMPT"] = "0",
            ["GCM_INTERACTIVE"] = "Never",
            ["GIT_EDITOR"] = "false",
            ["GIT_SEQUENCE_EDITOR"] = "false",
            ["GIT_PAGER"] = "cat",
            ["GIT_LFS_SKIP_SMUDGE"] = "1",
        };
    }

    private static string QuoteFilterExecutable(string path)
    {
        var normalized = OperatingSystem.IsWindows() ? path.Replace('\\', '/') : path;
        return $"'{normalized.Replace("'", "'\\''", StringComparison.Ordinal)}'";
    }
}
