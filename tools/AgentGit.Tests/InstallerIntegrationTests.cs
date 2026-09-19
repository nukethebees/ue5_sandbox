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
        var result = RunInstaller(source_root, fixture.RepositoryRoot, install_root, skip_validation: true);

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
            policy_path: "../.agent-git.json",
            skip_validation: true);

        Assert.AreNotEqual(0, result.ExitCode);
        StringAssert.Contains(result.Error, "PolicyPath must be a safe repository-relative Git path");
        Assert.IsFalse(Directory.Exists(install_root));
    }

    [TestMethod]
    public void Failed_preinstall_validation_preserves_existing_trusted_installation()
    {
        using var fixture = new TemporaryAgentGitRepository();
        var source_root = FindSourceRoot();
        var install_root = Directory.CreateDirectory(
            Path.Combine(fixture.InstallRoot, "existing-agent-git")).FullName;
        var marker = Path.Combine(install_root, "known-good.marker");
        File.WriteAllText(marker, "known-good");
        var invalid_project = Path.Combine(fixture.InstallRoot, "invalid-validation.csproj");
        File.WriteAllText(invalid_project, "<Project><Invalid></Project>");

        var result = RunInstaller(
            source_root,
            fixture.RepositoryRoot,
            install_root,
            validation_project: invalid_project);

        Assert.AreNotEqual(0, result.ExitCode);
        StringAssert.Contains(result.Error, "validation build failed");
        Assert.AreEqual("known-good", File.ReadAllText(marker));
        Assert.IsFalse(File.Exists(Path.Combine(install_root, "bin", "agent-git.exe")));
        Assert.IsFalse(Directory.EnumerateDirectories(
            fixture.InstallRoot,
            "existing-agent-git.staging.*").Any());
    }

    [TestMethod]
    public void Test_only_validation_bypass_cannot_target_a_non_temporary_installation()
    {
        using var fixture = new TemporaryAgentGitRepository();
        var source_root = FindSourceRoot();
        var install_root = Path.Combine(source_root, "rejected-agent-git-test-install");

        var result = RunInstaller(
            source_root,
            fixture.RepositoryRoot,
            install_root,
            skip_validation: true);

        Assert.AreNotEqual(0, result.ExitCode);
        StringAssert.Contains(result.Error, "restricted to test installations");
        Assert.IsFalse(Directory.Exists(install_root));
    }

    private static InstallerResult RunInstaller(
        string source_root,
        string repository_root,
        string install_root,
        string policy_path = ".agent-git.json",
        string? validation_project = null,
        bool skip_validation = false)
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
        if (validation_project is not null)
        {
            start_info.ArgumentList.Add("-ValidationProject");
            start_info.ArgumentList.Add(validation_project);
        }
        if (skip_validation)
        {
            start_info.ArgumentList.Add("-SkipValidation");
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
