using System.Diagnostics;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace AgentGitInstaller.Tests;

internal sealed class TemporaryInstallerRepository : IDisposable
{
    private readonly string root;

    public TemporaryInstallerRepository(string repository_id = "test/repository", bool include_lfs = false)
    {
        root = Directory.CreateTempSubdirectory("AgentGitInstaller-").FullName;
        RepositoryRoot = Directory.CreateDirectory(Path.Combine(root, "repository")).FullName;
        InstallParent = Directory.CreateDirectory(Path.Combine(root, "install")).FullName;
        GitExecutable = FindExecutable(OperatingSystem.IsWindows() ? "git.exe" : "git");

        RunGit("init", "-q", "--initial-branch=dev");
        RunGit("config", "user.name", "Agent Git Installer Test");
        RunGit("config", "user.email", "agent-git-installer@example.com");
        RunGit("remote", "add", "origin", $"https://example.invalid/{Guid.NewGuid():N}.git");
        WritePolicy(repository_id, "dev", include_lfs);
        File.WriteAllText(Path.Combine(RepositoryRoot, "README.md"), "initial\n");
        RunGit("add", "--all");
        RunGit("commit", "-qm", "initial");
    }

    public string RepositoryRoot { get; }

    public string InstallParent { get; }

    public string GitExecutable { get; }

    public string CommonGitDirectory => RunGit("rev-parse", "--path-format=absolute", "--git-common-dir").Trim();

    public void WritePolicy(string repository_id, string base_branch, bool include_lfs = false)
    {
        File.WriteAllText(
            Path.Combine(RepositoryRoot, ".agent-git.json"),
            ValidPolicy(repository_id, base_branch, include_lfs));
    }

    public void CommitPolicy(string repository_id, string base_branch, bool include_lfs = false)
    {
        WritePolicy(repository_id, base_branch, include_lfs);
        RunGit("add", ".agent-git.json");
        RunGit("commit", "-qm", "update policy");
    }

    public string RunGit(params string[] arguments)
    {
        var start_info = new ProcessStartInfo
        {
            FileName = GitExecutable,
            WorkingDirectory = RepositoryRoot,
            UseShellExecute = false,
            RedirectStandardOutput = true,
            RedirectStandardError = true,
            CreateNoWindow = true,
        };
        start_info.Environment["GIT_CONFIG_NOSYSTEM"] = "1";
        start_info.Environment["GIT_CONFIG_GLOBAL"] = Path.Combine(root, "empty.gitconfig");
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
                $"Git {string.Join(' ', arguments)} failed with exit code {process.ExitCode}: {error}");
        }

        return output;
    }

    public void Dispose()
    {
        if (!Directory.Exists(root))
        {
            return;
        }

        foreach (var file in Directory.EnumerateFiles(root, "*", SearchOption.AllDirectories))
        {
            File.SetAttributes(file, FileAttributes.Normal);
        }
        Directory.Delete(root, recursive: true);
    }

    public static string ValidPolicy(string repository_id, string base_branch, bool include_lfs)
    {
        return $$"""
            {
              "version": 1,
              "repositoryId": "{{repository_id}}",
              "baseBranch": "{{base_branch}}",
              "branchGroups": {
                "protected": [{ "exact": "dev" }, { "exact": "main" }],
                "workspace": [{ "regex": "^dev[0-9]+$" }]
              },
              "gitExtensions": {{(include_lfs ? "[\"lfs\"]" : "[]")}},
              "policies": {
                "add": { "allowedCurrentGroups": ["feature"] },
                "addAll": { "allowedCurrentGroups": ["feature"] },
                "commit": { "allowedCurrentGroups": ["feature"] },
                "switch": {
                  "allowedCurrentGroups": ["workspace", "feature"],
                  "allowedTargetGroups": ["workspace", "feature"],
                  "requireClean": true
                },
                "switchCreate": {
                  "allowedCurrentGroups": ["workspace", "feature"],
                  "allowedTargetGroups": ["feature"]
                },
                "rebaseBase": {
                  "allowedCurrentGroups": ["feature"],
                  "requireClean": true
                },
                "branchDelete": {
                  "allowedCurrentGroups": ["protected", "workspace", "feature"],
                  "allowedTargetGroups": ["feature"],
                  "requireClean": true,
                  "requireMergedInto": "base"
                }
              }
            }
            """;
    }

    private static string FindExecutable(string name)
    {
        foreach (var directory in (Environment.GetEnvironmentVariable("PATH") ?? string.Empty)
                     .Split(Path.PathSeparator, StringSplitOptions.RemoveEmptyEntries))
        {
            var candidate = Path.Combine(directory.Trim('"'), name);
            if (File.Exists(candidate))
            {
                return Path.GetFullPath(candidate);
            }
        }

        throw new AssertFailedException($"Unable to locate '{name}' on PATH.");
    }
}
