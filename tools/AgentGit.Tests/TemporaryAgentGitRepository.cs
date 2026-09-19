using System.Diagnostics;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace AgentGit.Tests;

internal sealed class TemporaryAgentGitRepository : IDisposable
{
    private const string origin_url = "https://example.invalid/agent-git-test.git";

    private readonly string root;

    public TemporaryAgentGitRepository()
    {
        root = Directory.CreateTempSubdirectory("AgentGit-").FullName;
        RepositoryRoot = Directory.CreateDirectory(Path.Combine(root, "main")).FullName;
        InstallRoot = Directory.CreateDirectory(Path.Combine(root, "install")).FullName;
        GitExecutable = FindExecutable("git.exe");

        RunGitAt(RepositoryRoot, "init", "-q", "--initial-branch=dev");
        RunGit("config", "user.name", "Agent Git Test");
        RunGit("config", "user.email", "agent-git@example.com");
        RunGit("remote", "add", "origin", origin_url);
        WriteFile(".agent-git.json", PolicyLoaderTests.ValidPolicy());
        WriteFile("README.md", "initial\n");
        RunGit("add", "--all");
        RunGit("commit", "-qm", "initial");

        var config = Path.Combine(InstallRoot, "empty.gitconfig");
        var attributes = Path.Combine(InstallRoot, "config", "empty.attributes");
        Directory.CreateDirectory(Path.GetDirectoryName(attributes)!);
        File.WriteAllText(config, string.Empty);
        File.WriteAllText(attributes, string.Empty);
        var hooks = Directory.CreateDirectory(Path.Combine(InstallRoot, "hooks")).FullName;
        var common_git_directory = RunGit("rev-parse", "--path-format=absolute", "--git-common-dir").Trim();
        Trust = new TrustContext(
            InstallRoot,
            GitExecutable,
            null,
            "Agent Git Test",
            "agent-git@example.com",
            config,
            hooks,
            [
                new TrustedRepository
                {
                    RepositoryId = "test/repository",
                    CommonGitDirectory = common_git_directory,
                    OriginUrl = origin_url,
                    PolicyRef = "refs/heads/dev",
                    PolicyPath = ".agent-git.json",
                },
            ]);
    }

    public string RepositoryRoot { get; }

    public string InstallRoot { get; }

    public string GitExecutable { get; }

    public TrustContext Trust { get; }

    public string WriteFile(string relative_path, string contents, string? worktree = null)
    {
        var path = Path.GetFullPath(Path.Combine(worktree ?? RepositoryRoot, relative_path));
        Directory.CreateDirectory(Path.GetDirectoryName(path)!);
        File.WriteAllText(path, contents);
        return path;
    }

    public string CreateWorktree(string branch, string directory_name = "dev1", string start_point = "dev")
    {
        RunGit("branch", branch, start_point);
        var path = Path.Combine(root, directory_name);
        RunGit("worktree", "add", "-q", path, branch);
        return path;
    }

    public string RunGit(params string[] arguments) => RunGitAt(RepositoryRoot, arguments);

    public string RunGitAt(string working_directory, params string[] arguments)
    {
        var start_info = new ProcessStartInfo
        {
            FileName = GitExecutable,
            WorkingDirectory = working_directory,
            UseShellExecute = false,
            RedirectStandardOutput = true,
            RedirectStandardError = true,
            CreateNoWindow = true,
        };
        start_info.Environment["GIT_CONFIG_NOSYSTEM"] = "1";
        start_info.Environment["GIT_CONFIG_GLOBAL"] = Path.Combine(InstallRoot, "fixture-empty.gitconfig");
        foreach (var argument in arguments)
        {
            start_info.ArgumentList.Add(argument);
        }

        using var process = Process.Start(start_info) ?? throw new AssertFailedException("Unable to start Git.");
        var output = process.StandardOutput.ReadToEnd();
        var error = process.StandardError.ReadToEnd();
        process.WaitForExit();
        if (process.ExitCode != 0)
        {
            throw new AssertFailedException(
                $"Git {string.Join(' ', arguments.Select(argument => $"[{argument}]"))} failed with " +
                $"code {process.ExitCode}: {error}");
        }

        return output;
    }

    public async Task<ApplicationResult> RunAgentGitAsync(string working_directory, params string[] arguments)
    {
        if (!CommandLine.TryParse(arguments, out var request, out var parse_error))
        {
            throw new AssertFailedException($"Test command did not parse: {parse_error}");
        }

        var output = new StringWriter();
        var error = new StringWriter();
        var git = new GitClient(Trust, new ProcessRunner());
        var discovery = new RepositoryDiscovery(git);
        var application = new AgentGitApplication(
            Trust,
            discovery,
            new PolicyEvaluator(discovery),
            new OperationExecutor(git),
            output,
            error);
        var exit_code = await application.RunAsync(request!, working_directory);
        return new ApplicationResult(exit_code, output.ToString(), error.ToString());
    }

    public void Dispose()
    {
        foreach (var file_path in Directory.EnumerateFiles(root, "*", SearchOption.AllDirectories))
        {
            File.SetAttributes(file_path, FileAttributes.Normal);
        }

        Directory.Delete(root, recursive: true);
    }

    private static string FindExecutable(string name)
    {
        var path = Environment.GetEnvironmentVariable("PATH") ?? string.Empty;
        foreach (var directory in path.Split(Path.PathSeparator, StringSplitOptions.RemoveEmptyEntries))
        {
            var candidate = Path.Combine(directory.Trim('"'), name);
            if (File.Exists(candidate))
            {
                return Path.GetFullPath(candidate);
            }
        }

        throw new AssertFailedException($"Unable to locate {name} on PATH.");
    }
}

internal sealed record ApplicationResult(int ExitCode, string Output, string Error);
