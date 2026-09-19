using System.Diagnostics;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace AgentGit.Tests;

[TestClass]
public sealed class TrustStoreTests
{
    [TestMethod]
    public void LoadCanonical_rejects_repository_build_output_before_loading_manifest()
    {
        var repository_binary = typeof(Program).Assembly.Location;

        var exception = Assert.ThrowsException<PolicyConfigurationException>(
            () => TrustStore.LoadCanonical(repository_binary));

        StringAssert.Contains(exception.Message, "Repository build outputs are not trusted");
    }

    [TestMethod]
    public void FindRegistration_rejects_missing_and_duplicate_repository_identities()
    {
        var registration = new TrustedRepository
        {
            RepositoryId = "test/repository",
            CommonGitDirectory = Path.GetFullPath("repository.git"),
            OriginUrl = "https://example.invalid/repository.git",
            PolicyRef = "refs/heads/dev",
            PolicyPath = ".agent-git.json",
        };
        var trust = new TrustContext("install", "git", null, "Test", "test@example.com", "config", "hooks", [registration]);

        Assert.AreSame(registration, trust.FindRegistration(registration.CommonGitDirectory));
        Assert.ThrowsException<RepositoryException>(() => trust.FindRegistration(Path.GetFullPath("other.git")));

        var duplicate = trust with { Repositories = [registration, registration] };
        Assert.ThrowsException<PolicyConfigurationException>(() => duplicate.FindRegistration(registration.CommonGitDirectory));
    }

    [TestMethod]
    public void ValidateManifest_rejects_unsafe_policy_locations_and_duplicate_registrations()
    {
        using var fixture = new TemporaryAgentGitRepository();
        var registration = fixture.Trust.Repositories.Single();
        var manifest = CreateManifest(fixture, registration);
        TrustStore.ValidateManifest(manifest);

        var unsafe_ref = CopyRegistration(registration, policy_ref: "refs/heads/dev:attacker");
        Assert.ThrowsException<PolicyConfigurationException>(
            () => TrustStore.ValidateManifest(CreateManifest(fixture, unsafe_ref)));

        var unsafe_path = CopyRegistration(registration, policy_path: "../.agent-git.json");
        Assert.ThrowsException<PolicyConfigurationException>(
            () => TrustStore.ValidateManifest(CreateManifest(fixture, unsafe_path)));

        Assert.ThrowsException<PolicyConfigurationException>(
            () => TrustStore.ValidateManifest(CreateManifest(fixture, registration, registration)));
    }

    [TestMethod]
    public void ParseManifest_rejects_duplicate_json_properties()
    {
        var exception = Assert.ThrowsException<PolicyConfigurationException>(() => TrustStore.ParseManifest(
            """
            {
              "version": 1,
              "version": 1,
              "gitExecutable": "unused",
              "userName": "Test",
              "userEmail": "test@example.com",
              "repositories": []
            }
            """));

        StringAssert.Contains(exception.Message, "Duplicate JSON property 'version'");
    }

    [TestMethod]
    public void ValidateIsolationLayout_reports_invalid_empty_files_and_hook_directory()
    {
        var root = Directory.CreateTempSubdirectory("AgentGitTrust-").FullName;
        try
        {
            Directory.CreateDirectory(Path.Combine(root, "bin"));
            var config = Directory.CreateDirectory(Path.Combine(root, "config")).FullName;
            var empty_config = Path.Combine(config, "empty.gitconfig");
            var empty_attributes = Path.Combine(config, "empty.attributes");
            var hooks = Directory.CreateDirectory(Path.Combine(config, "empty-hooks")).FullName;
            File.WriteAllText(empty_config, string.Empty);
            File.WriteAllText(empty_attributes, string.Empty);
            TrustStore.ValidateIsolationLayout(root);

            File.Delete(empty_config);
            AssertIsolationFailure(root, empty_config, "zero-byte regular file", "does not exist");
            File.WriteAllText(empty_config, string.Empty);

            File.WriteAllText(empty_config, "[alias]\nstatus = !arbitrary-command\n");
            AssertIsolationFailure(root, empty_config, "zero-byte regular file", "bytes");

            File.WriteAllText(empty_config, string.Empty);
            File.Delete(empty_attributes);
            AssertIsolationFailure(root, empty_attributes, "zero-byte regular file", "does not exist");
            File.WriteAllText(empty_attributes, string.Empty);

            File.WriteAllText(empty_attributes, "*.txt text\n");
            AssertIsolationFailure(root, empty_attributes, "zero-byte regular file", "bytes");

            File.WriteAllText(empty_attributes, string.Empty);
            Directory.Delete(hooks);
            AssertIsolationFailure(root, hooks, "empty directory", "does not exist");
            Directory.CreateDirectory(hooks);

            File.WriteAllText(Path.Combine(hooks, "post-commit"), "arbitrary-command\n");
            AssertIsolationFailure(root, hooks, "empty directory", "post-commit");
        }
        finally
        {
            Directory.Delete(root, recursive: true);
        }
    }

    [TestMethod]
    public void ValidateIsolationLayout_reports_reparse_point_directory()
    {
        var root = Directory.CreateTempSubdirectory("AgentGitTrust-").FullName;
        var config = Path.Combine(root, "config");
        try
        {
            Directory.CreateDirectory(Path.Combine(root, "bin"));
            var target = Directory.CreateDirectory(Path.Combine(root, "config-target")).FullName;
            File.WriteAllText(Path.Combine(target, "empty.gitconfig"), string.Empty);
            File.WriteAllText(Path.Combine(target, "empty.attributes"), string.Empty);
            Directory.CreateDirectory(Path.Combine(target, "empty-hooks"));
            CreateDirectoryLink(config, target);

            AssertIsolationFailure(root, config, "normal directory", "reparse point");
        }
        finally
        {
            if (Directory.Exists(config))
            {
                RemoveDirectoryLink(config);
            }

            Directory.Delete(root, recursive: true);
        }
    }

    private static void AssertIsolationFailure(string root, string path, params string[] expected_fragments)
    {
        var exception = Assert.ThrowsException<PolicyConfigurationException>(
            () => TrustStore.ValidateIsolationLayout(root));

        StringAssert.Contains(exception.Message, path);
        foreach (var expected_fragment in expected_fragments)
        {
            StringAssert.Contains(exception.Message, expected_fragment);
        }
    }

    private static void CreateDirectoryLink(string link, string target)
    {
        if (!OperatingSystem.IsWindows())
        {
            Directory.CreateSymbolicLink(link, target);
            return;
        }

        var start_info = new ProcessStartInfo
        {
            FileName = "cmd.exe",
            UseShellExecute = false,
            RedirectStandardOutput = true,
            RedirectStandardError = true,
            CreateNoWindow = true,
        };
        foreach (var argument in new[] { "/d", "/c", "mklink", "/J", link, target })
        {
            start_info.ArgumentList.Add(argument);
        }

        using var process = Process.Start(start_info) ?? throw new IOException("Unable to start junction test setup.");
        var output = process.StandardOutput.ReadToEnd();
        var error = process.StandardError.ReadToEnd();
        process.WaitForExit();
        if (process.ExitCode != 0)
        {
            throw new IOException($"Unable to create test junction: {output}{error}");
        }
    }

    private static void RemoveDirectoryLink(string link)
    {
        if (!OperatingSystem.IsWindows())
        {
            Directory.Delete(link);
            return;
        }

        var start_info = new ProcessStartInfo
        {
            FileName = "cmd.exe",
            UseShellExecute = false,
            RedirectStandardOutput = true,
            RedirectStandardError = true,
            CreateNoWindow = true,
        };
        foreach (var argument in new[] { "/d", "/c", "rmdir", link })
        {
            start_info.ArgumentList.Add(argument);
        }

        using var process = Process.Start(start_info) ?? throw new IOException("Unable to start junction test cleanup.");
        process.WaitForExit();
        if (process.ExitCode != 0)
        {
            throw new IOException($"Unable to remove test junction: {process.StandardError.ReadToEnd()}");
        }
    }

    private static TrustManifest CreateManifest(
        TemporaryAgentGitRepository fixture,
        params TrustedRepository[] repositories)
    {
        return new TrustManifest
        {
            Version = 1,
            GitExecutable = fixture.GitExecutable,
            UserName = "Agent Git Test",
            UserEmail = "agent-git@example.com",
            Repositories = repositories,
        };
    }

    private static TrustedRepository CopyRegistration(
        TrustedRepository source,
        string? policy_ref = null,
        string? policy_path = null)
    {
        return new TrustedRepository
        {
            RepositoryId = source.RepositoryId,
            CommonGitDirectory = source.CommonGitDirectory,
            OriginUrl = source.OriginUrl,
            PolicyRef = policy_ref ?? source.PolicyRef,
            PolicyPath = policy_path ?? source.PolicyPath,
        };
    }
}
