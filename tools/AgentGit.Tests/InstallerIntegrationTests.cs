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
        var installer = Path.Combine(source_root, "PowerShell", "Install-AgentGit.ps1");
        var install_root = Path.Combine(fixture.InstallRoot, "installed-agent-git");
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
                     "-Repository", fixture.RepositoryRoot,
                     "-BaseBranch", "dev",
                     "-InstallRoot", install_root,
                 })
        {
            start_info.ArgumentList.Add(argument);
        }

        using var process = Process.Start(start_info) ?? throw new AssertFailedException("Unable to start installer.");
        var output = process.StandardOutput.ReadToEnd();
        var error = process.StandardError.ReadToEnd();
        process.WaitForExit();

        Assert.AreEqual(0, process.ExitCode, error);
        Assert.IsTrue(File.Exists(Path.Combine(install_root, "bin", "agent-git.exe")));
        Assert.IsTrue(File.Exists(Path.Combine(install_root, "config", "empty.gitconfig")));
        Assert.IsTrue(Directory.Exists(Path.Combine(install_root, "config", "empty-hooks")));
        using var manifest = JsonDocument.Parse(File.ReadAllText(Path.Combine(install_root, "trust.json")));
        var repository = manifest.RootElement.GetProperty("repositories")[0];
        Assert.AreEqual("test/repository", repository.GetProperty("repositoryId").GetString());
        Assert.AreEqual("refs/heads/dev", repository.GetProperty("policyRef").GetString());
        StringAssert.Contains(output, "Registered repository");
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
}
