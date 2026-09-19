using System.Diagnostics;
using System.Text.Json;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace AgentGit.Tests;

[TestClass]
public sealed class InstallerIntegrationTests
{
    [TestMethod]
    public void Installer_stages_canonical_layout_and_registers_protected_policy_ref()
    {
        using var fixture = new TemporaryAgentGitRepository();
        var source_root = FindSourceRoot();
        var install_root = Path.Combine(fixture.InstallRoot, "installed-agent-git");
        var result = RunInstaller(source_root, fixture.RepositoryRoot, install_root);

        Assert.AreEqual(0, result.ExitCode, result.Error);
        Assert.IsTrue(File.Exists(Path.Combine(install_root, "bin", "agent-git.exe")));
        Assert.IsTrue(File.Exists(Path.Combine(install_root, "config", "empty.gitconfig")));
        Assert.IsTrue(Directory.Exists(Path.Combine(install_root, "config", "empty-hooks")));
        using var manifest = JsonDocument.Parse(File.ReadAllText(Path.Combine(install_root, "trust.json")));
        var repository = manifest.RootElement.GetProperty("repositories")[0];
        Assert.AreEqual("test/repository", repository.GetProperty("repositoryId").GetString());
        Assert.AreEqual("refs/heads/dev", repository.GetProperty("policyRef").GetString());
        StringAssert.Contains(result.Output, "Registered repository");
    }

    [TestMethod]
    public void Installer_rejects_unsafe_policy_path_without_activating_installation()
    {
        using var fixture = new TemporaryAgentGitRepository();
        var source_root = FindSourceRoot();
        var install_root = Path.Combine(fixture.InstallRoot, "rejected-agent-git");
        var result = RunInstaller(
            source_root,
            fixture.RepositoryRoot,
            install_root,
            "../.agent-git.json");

        Assert.AreNotEqual(0, result.ExitCode);
        StringAssert.Contains(result.Error, "PolicyPath must be a safe repository-relative Git path");
        Assert.IsFalse(Directory.Exists(install_root));
    }

    private static InstallerResult RunInstaller(
        string source_root,
        string repository_root,
        string install_root,
        string policy_path = ".agent-git.json")
    {
        var installer = Path.Combine(source_root, "PowerShell", "Install-AgentGit.ps1");
        var start_info = new ProcessStartInfo
        {
            FileName = "pwsh",
            WorkingDirectory = source_root,
            UseShellExecute = false,
            RedirectStandardOutput = true,
            RedirectStandardError = true,
            CreateNoWindow = true,
        };
        foreach (var argument in new[]
                 {
                     "-NoProfile",
                     "-File", installer,
                     "-Repository", repository_root,
                     "-BaseBranch", "dev",
                     "-PolicyPath", policy_path,
                     "-InstallRoot", install_root,
                 })
        {
            start_info.ArgumentList.Add(argument);
        }

        using var process = Process.Start(start_info) ?? throw new AssertFailedException("Unable to start installer.");
        var output = process.StandardOutput.ReadToEnd();
        var error = process.StandardError.ReadToEnd();
        process.WaitForExit();
        return new InstallerResult(process.ExitCode, output, error);
    }

    private static string FindSourceRoot()
    {
        for (var directory = new DirectoryInfo(AppContext.BaseDirectory); directory is not null; directory = directory.Parent)
        {
            if (File.Exists(Path.Combine(directory.FullName, "dev.ps1")) &&
                File.Exists(Path.Combine(directory.FullName, "tools", "Tools.slnx")))
            {
                return directory.FullName;
            }
        }

        throw new AssertFailedException("Unable to locate repository root from test output directory.");
    }

    private sealed record InstallerResult(int ExitCode, string Output, string Error);
}
