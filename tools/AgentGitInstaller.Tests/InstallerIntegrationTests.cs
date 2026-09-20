using System.Diagnostics;
using System.Security;
using System.Security.Cryptography;
using System.Text.Json;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace AgentGitInstaller.Tests;

[TestClass]
public sealed class InstallerIntegrationTests
{
    [TestMethod]
    public async Task Fresh_install_stages_layout_and_trusted_policy_ref()
    {
        using var fixture = new TemporaryInstallerRepository();
        var install_root = Path.Combine(fixture.InstallParent, "fresh-agent-git");

        await InstallFromTestArtifactsAsync(fixture, install_root);

        Assert.IsTrue(File.Exists(Path.Combine(install_root, "bin", "agent-git.exe")));
        Assert.AreEqual(0, new FileInfo(Path.Combine(install_root, "config", "empty-hooks")).Length);
        using var manifest = LoadManifest(install_root);
        var registration = manifest.RootElement.GetProperty("repositories")[0];
        Assert.AreEqual("test/repository", registration.GetProperty("repositoryId").GetString());
        Assert.AreEqual("refs/heads/dev", registration.GetProperty("policyRef").GetString());
        AssertNoTransientDirectories(fixture.InstallParent, "fresh-agent-git");
    }

    [TestMethod]
    public async Task Replacement_preserves_previous_install_and_updates_current_registration()
    {
        using var fixture = new TemporaryInstallerRepository("test/original");
        var install_root = Path.Combine(fixture.InstallParent, "replacement-agent-git");
        await InstallFromTestArtifactsAsync(fixture, install_root);
        File.WriteAllText(Path.Combine(install_root, "first-install.marker"), "known-good");
        fixture.CommitPolicy("test/updated", "dev");

        await InstallFromTestArtifactsAsync(fixture, install_root);

        var previous_root = Path.Combine(fixture.InstallParent, "replacement-agent-git.previous");
        Assert.AreEqual("known-good", File.ReadAllText(Path.Combine(previous_root, "first-install.marker")));
        using var manifest = LoadManifest(install_root);
        var repositories = manifest.RootElement.GetProperty("repositories");
        Assert.AreEqual(1, repositories.GetArrayLength());
        Assert.AreEqual("test/updated", repositories[0].GetProperty("repositoryId").GetString());
    }

    [TestMethod]
    public async Task Trust_manifest_preserves_multiple_registered_repositories()
    {
        using var first = new TemporaryInstallerRepository("test/first");
        using var second = new TemporaryInstallerRepository("test/second");
        var install_root = Path.Combine(first.InstallParent, "multiple-agent-git");
        await InstallFromTestArtifactsAsync(first, install_root);
        await InstallFromTestArtifactsAsync(second, install_root);

        using var manifest = LoadManifest(install_root);
        var repositories = manifest.RootElement.GetProperty("repositories");
        Assert.AreEqual(2, repositories.GetArrayLength());
        CollectionAssert.AreEquivalent(
            new[] { "test/first", "test/second" },
            repositories.EnumerateArray()
                .Select(item => item.GetProperty("repositoryId").GetString())
                .ToArray());
    }

    [TestMethod]
    public async Task Policy_is_loaded_from_requested_ref_not_working_tree()
    {
        using var fixture = new TemporaryInstallerRepository();
        fixture.WritePolicy("attacker/uncommitted", "other");
        var install_root = Path.Combine(fixture.InstallParent, "ref-policy-agent-git");

        await InstallFromTestArtifactsAsync(fixture, install_root);

        using var manifest = LoadManifest(install_root);
        Assert.AreEqual(
            "test/repository",
            manifest.RootElement.GetProperty("repositories")[0].GetProperty("repositoryId").GetString());
    }

    [TestMethod]
    public async Task Policy_object_is_pinned_when_branch_moves_after_resolution()
    {
        using var fixture = new TemporaryInstallerRepository("test/original");
        var original_commit = fixture.RunGit("rev-parse", "HEAD").Trim();
        fixture.CommitPolicy("test/moved", "dev");
        var moved_commit = fixture.RunGit("rev-parse", "HEAD").Trim();
        fixture.RunGit("reset", "--hard", original_commit);
        var install_root = Path.Combine(fixture.InstallParent, "moved-policy-agent-git");
        var runner = new MoveBranchAfterResolutionProcessRunner(new ProcessRunner(), fixture, moved_commit);

        await CreateApplication(runner).InstallAsync(CreateRequest(fixture, install_root), CancellationToken.None);

        using var manifest = LoadManifest(install_root);
        Assert.AreEqual(
            "test/original",
            manifest.RootElement.GetProperty("repositories")[0].GetProperty("repositoryId").GetString());
    }

    [TestMethod]
    public async Task Rejects_unsafe_and_malformed_policy_without_installing()
    {
        using var fixture = new TemporaryInstallerRepository();
        var unsafe_root = Path.Combine(fixture.InstallParent, "unsafe-agent-git");
        var unsafe_exception = await Assert.ThrowsExceptionAsync<InstallerException>(() =>
            CreateApplication().InstallAsync(
                CreateRequest(fixture, unsafe_root) with { PolicyPath = "../.agent-git.json" },
                CancellationToken.None));
        StringAssert.Contains(unsafe_exception.Message, "safe repository-relative Git path");
        Assert.IsFalse(Directory.Exists(unsafe_root));

        File.WriteAllText(Path.Combine(fixture.RepositoryRoot, ".agent-git.json"), "{ malformed");
        fixture.RunGit("add", ".agent-git.json");
        fixture.RunGit("commit", "-qm", "malformed policy");
        var malformed_root = Path.Combine(fixture.InstallParent, "malformed-agent-git");
        await Assert.ThrowsExceptionAsync<AgentGit.PolicyConfigurationException>(() =>
            InstallFromTestArtifactsAsync(fixture, malformed_root));
        Assert.IsFalse(Directory.Exists(malformed_root));
    }

    [TestMethod]
    public async Task Rejects_policy_base_branch_disagreement()
    {
        using var fixture = new TemporaryInstallerRepository();
        fixture.CommitPolicy("test/repository", "main");
        var install_root = Path.Combine(fixture.InstallParent, "wrong-base-agent-git");

        var exception = await Assert.ThrowsExceptionAsync<InstallerException>(() =>
            InstallFromTestArtifactsAsync(fixture, install_root));

        StringAssert.Contains(exception.Message, "does not match requested base");
        Assert.IsFalse(Directory.Exists(install_root));
    }

    [TestMethod]
    public async Task Test_controls_require_strict_temporary_install_root()
    {
        using var fixture = new TemporaryInstallerRepository();
        var outside_temp = Path.Combine(FindSourceRoot(), "rejected-agent-git-test-install");
        var exception = await Assert.ThrowsExceptionAsync<InstallerException>(() =>
            InstallFromTestArtifactsAsync(fixture, outside_temp));
        StringAssert.Contains(exception.Message, "restricted to test installations");
        Assert.IsFalse(Directory.Exists(outside_temp));

        var temp_root_exception = await Assert.ThrowsExceptionAsync<InstallerException>(() =>
            InstallFromTestArtifactsAsync(fixture, Path.GetTempPath()));
        StringAssert.Contains(temp_root_exception.Message, "restricted to test installations");
    }

    [TestMethod]
    public async Task Missing_runtime_artifact_is_rejected_before_activation()
    {
        using var fixture = new TemporaryInstallerRepository();
        var artifact_root = Directory.CreateDirectory(Path.Combine(fixture.InstallParent, "incomplete-artifacts")).FullName;
        var install_root = Path.Combine(fixture.InstallParent, "missing-artifact-agent-git");
        var request = CreateRequest(fixture, install_root) with { TestArtifactRoot = artifact_root };

        var exception = await Assert.ThrowsExceptionAsync<InstallerException>(() =>
            CreateApplication().InstallAsync(request, CancellationToken.None));

        StringAssert.Contains(exception.Message, "runtime artifact is missing");
        Assert.IsFalse(Directory.Exists(install_root));
    }

    [TestMethod]
    public async Task Post_install_failure_restores_existing_installation_and_cleans_state()
    {
        using var fixture = new TemporaryInstallerRepository();
        var install_root = Directory.CreateDirectory(
            Path.Combine(fixture.InstallParent, "rollback-agent-git")).FullName;
        var marker = Path.Combine(install_root, "known-good.marker");
        File.WriteAllText(marker, "known-good");
        var request = CreateRequest(fixture, install_root) with { TestFailPostInstall = true };

        var exception = await Assert.ThrowsExceptionAsync<InstallerException>(() =>
            CreateApplication().InstallAsync(request, CancellationToken.None));

        StringAssert.Contains(exception.Message, "Test-only post-install failure");
        Assert.AreEqual("known-good", File.ReadAllText(marker));
        Assert.IsFalse(File.Exists(Path.Combine(install_root, "bin", "agent-git.exe")));
        AssertNoTransientDirectories(fixture.InstallParent, "rollback-agent-git");
    }

    [TestMethod]
    public async Task Unsupported_git_version_is_rejected_through_process_seam()
    {
        using var fixture = new TemporaryInstallerRepository();
        var runner = new GitVersionProcessRunner(new ProcessRunner(), "git version 2.37.9\n");
        var application = CreateApplication(runner);

        var exception = await Assert.ThrowsExceptionAsync<InstallerException>(() =>
            application.InstallAsync(
                CreateRequest(fixture, Path.Combine(fixture.InstallParent, "old-git-agent-git")),
                CancellationToken.None));

        StringAssert.Contains(exception.Message, "requires Git 2.38 or newer");
    }

    [TestMethod]
    public async Task Version_smoke_failure_rolls_back_existing_installation()
    {
        using var fixture = new TemporaryInstallerRepository();
        var install_root = Directory.CreateDirectory(
            Path.Combine(fixture.InstallParent, "smoke-failure-agent-git")).FullName;
        var marker = Path.Combine(install_root, "known-good.marker");
        File.WriteAllText(marker, "known-good");
        var runner = new SmokeFailureProcessRunner(new ProcessRunner());

        var exception = await Assert.ThrowsExceptionAsync<InstallerException>(() =>
            CreateApplication(runner).InstallAsync(CreateRequest(fixture, install_root), CancellationToken.None));

        StringAssert.Contains(exception.Message, "version smoke test");
        Assert.AreEqual("known-good", File.ReadAllText(marker));
        AssertNoTransientDirectories(fixture.InstallParent, "smoke-failure-agent-git");
    }

    [TestMethod]
    public async Task Failed_validation_build_preserves_existing_installation()
    {
        using var fixture = new TemporaryInstallerRepository();
        var install_root = Directory.CreateDirectory(
            Path.Combine(fixture.InstallParent, "validation-failure-agent-git")).FullName;
        var marker = Path.Combine(install_root, "known-good.marker");
        File.WriteAllText(marker, "known-good");
        var invalid_project = Path.Combine(fixture.InstallParent, "invalid.csproj");
        File.WriteAllText(invalid_project, "<Project><Invalid></Project>");
        var request = CreateRequest(fixture, install_root) with
        {
            TestSkipValidation = false,
            TestArtifactRoot = null,
            TestValidationProject = invalid_project,
        };

        var exception = await Assert.ThrowsExceptionAsync<InstallerException>(() =>
            CreateApplication().InstallAsync(request, CancellationToken.None));

        StringAssert.Contains(exception.Message, "validation build failed");
        Assert.AreEqual("known-good", File.ReadAllText(marker));
        AssertNoTransientDirectories(fixture.InstallParent, "validation-failure-agent-git");
    }

    [TestMethod]
    public async Task Corrupted_validated_artifact_cannot_replace_existing_installation()
    {
        using var fixture = new TemporaryInstallerRepository();
        var install_root = Directory.CreateDirectory(
            Path.Combine(fixture.InstallParent, "corrupt-artifact-agent-git")).FullName;
        var marker = Path.Combine(install_root, "known-good.marker");
        File.WriteAllText(marker, "known-good");
        var validation_project = CreateValidationProject(fixture, test_succeeds: true);
        var request = CreateRequest(fixture, install_root) with
        {
            TestSkipValidation = false,
            TestArtifactRoot = null,
            TestValidationProject = validation_project,
            TestCorruptValidatedArtifact = true,
        };

        var exception = await Assert.ThrowsExceptionAsync<InstallerException>(() =>
            CreateApplication().InstallAsync(request, CancellationToken.None));

        StringAssert.Contains(exception.Message, "runtime artifact changed after validation");
        Assert.AreEqual("known-good", File.ReadAllText(marker));
        AssertNoTransientDirectories(fixture.InstallParent, "corrupt-artifact-agent-git");
    }

    [TestMethod]
    public async Task Failed_security_test_gate_does_not_activate_installation()
    {
        using var fixture = new TemporaryInstallerRepository();
        var install_root = Path.Combine(fixture.InstallParent, "test-gate-failure-agent-git");
        var validation_project = CreateValidationProject(fixture, test_succeeds: false);
        var request = CreateRequest(fixture, install_root) with
        {
            TestSkipValidation = false,
            TestArtifactRoot = null,
            TestValidationProject = validation_project,
        };

        var exception = await Assert.ThrowsExceptionAsync<InstallerException>(() =>
            CreateApplication().InstallAsync(request, CancellationToken.None));

        StringAssert.Contains(exception.Message, "security test gate failed");
        Assert.IsFalse(Directory.Exists(install_root));
        AssertNoTransientDirectories(fixture.InstallParent, "test-gate-failure-agent-git");
    }

    [TestMethod]
    public async Task Installed_executable_matches_validated_private_artifact()
    {
        using var fixture = new TemporaryInstallerRepository();
        var install_root = Path.Combine(fixture.InstallParent, "validated artifact with spaces");
        var validation_project = CreateValidationProject(fixture, test_succeeds: true);
        var request = CreateRequest(fixture, install_root) with
        {
            TestSkipValidation = false,
            TestArtifactRoot = null,
            TestValidationProject = validation_project,
        };
        var output = new StringWriter();
        var application = new AgentGitInstallerApplication(
            new ProcessRunner(),
            new ExecutableLocator(),
            new InstallerFileSystem(),
            new Sha256ArtifactHasher(),
            output);

        await application.InstallAsync(request, CancellationToken.None);

        var line = output.ToString().Split('\n', StringSplitOptions.RemoveEmptyEntries)
            .Single(value => value.Contains("SHA256 agent-git.exe=", StringComparison.Ordinal));
        var expected_hash = line[(line.LastIndexOf('=') + 1)..].Trim();
        var installed_hash = Convert.ToHexString(SHA256.HashData(
            File.ReadAllBytes(Path.Combine(install_root, "bin", "agent-git.exe"))));
        Assert.AreEqual(expected_hash, installed_hash);
        AssertNoTransientDirectories(fixture.InstallParent, "validated artifact with spaces");
    }

    [TestMethod]
    public async Task Reparse_point_artifact_root_is_rejected()
    {
        using var fixture = new TemporaryInstallerRepository();
        var link = Path.Combine(fixture.InstallParent, "linked-artifacts");
        CreateDirectoryLink(link, FindRuntimeArtifacts());
        try
        {
            var install_root = Path.Combine(fixture.InstallParent, "linked-artifact-agent-git");
            var request = CreateRequest(fixture, install_root) with { TestArtifactRoot = link };

            var exception = await Assert.ThrowsExceptionAsync<InstallerException>(() =>
                CreateApplication().InstallAsync(request, CancellationToken.None));

            StringAssert.Contains(exception.Message, "reparse point");
            Assert.IsFalse(Directory.Exists(install_root));
        }
        finally
        {
            RemoveDirectoryLink(link);
        }
    }

    [TestMethod]
    public async Task Canonical_installation_rejects_test_controls_without_modification()
    {
        using var fixture = new TemporaryInstallerRepository();
        var canonical_root = Path.GetFullPath(Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
            "NukeTheBees",
            "agent-git"));
        var before_write = Directory.GetLastWriteTimeUtc(canonical_root);

        var exception = await Assert.ThrowsExceptionAsync<InstallerException>(() =>
            InstallFromTestArtifactsAsync(fixture, canonical_root));

        StringAssert.Contains(exception.Message, "Canonical installation cannot skip or override");
        Assert.AreEqual(before_write, Directory.GetLastWriteTimeUtc(canonical_root));
    }

    [TestMethod]
    public async Task Non_repository_is_rejected_without_creating_installation()
    {
        using var fixture = new TemporaryInstallerRepository();
        var install_root = Path.Combine(fixture.InstallParent, "not-a-repository-agent-git");
        var request = CreateRequest(fixture, install_root) with { Repository = fixture.InstallParent };

        var exception = await Assert.ThrowsExceptionAsync<InstallerException>(() =>
            CreateApplication().InstallAsync(request, CancellationToken.None));

        StringAssert.Contains(exception.Message, "Git command failed");
        Assert.IsFalse(Directory.Exists(install_root));
    }

    [TestMethod]
    public async Task Malformed_existing_trust_manifest_is_not_silently_replaced()
    {
        using var fixture = new TemporaryInstallerRepository();
        var install_root = Directory.CreateDirectory(
            Path.Combine(fixture.InstallParent, "malformed-trust-agent-git")).FullName;
        var marker = Path.Combine(install_root, "known-good.marker");
        File.WriteAllText(marker, "known-good");
        File.WriteAllText(Path.Combine(install_root, "trust.json"), "{ malformed");

        await Assert.ThrowsExceptionAsync<AgentGit.PolicyConfigurationException>(() =>
            InstallFromTestArtifactsAsync(fixture, install_root));

        Assert.AreEqual("known-good", File.ReadAllText(marker));
        Assert.AreEqual("{ malformed", File.ReadAllText(Path.Combine(install_root, "trust.json")));
        AssertNoTransientDirectories(fixture.InstallParent, "malformed-trust-agent-git");
    }

    [TestMethod]
    public async Task Reparse_point_install_parent_is_rejected()
    {
        using var fixture = new TemporaryInstallerRepository();
        var target = Directory.CreateDirectory(Path.Combine(fixture.InstallParent, "install-target")).FullName;
        var link = Path.Combine(fixture.InstallParent, "install-link");
        CreateDirectoryLink(link, target);
        try
        {
            var install_root = Path.Combine(link, "agent-git");
            var exception = await Assert.ThrowsExceptionAsync<InstallerException>(() =>
                InstallFromTestArtifactsAsync(fixture, install_root));

            StringAssert.Contains(exception.Message, "reparse point");
            Assert.IsFalse(Directory.Exists(Path.Combine(target, "agent-git")));
        }
        finally
        {
            RemoveDirectoryLink(link);
        }
    }

    [TestMethod]
    public async Task Rollback_failure_reports_both_failures_and_preserves_recovery_paths()
    {
        using var fixture = new TemporaryInstallerRepository();
        var install_name = "rollback-failure-agent-git";
        var install_root = Directory.CreateDirectory(Path.Combine(fixture.InstallParent, install_name)).FullName;
        File.WriteAllText(Path.Combine(install_root, "known-good.marker"), "known-good");
        var file_system = new RestoreFailureFileSystem(new InstallerFileSystem());
        var request = CreateRequest(fixture, install_root) with { TestFailPostInstall = true };

        var exception = await Assert.ThrowsExceptionAsync<InstallerException>(() =>
            CreateApplication(file_system: file_system).InstallAsync(request, CancellationToken.None));

        StringAssert.Contains(exception.Message, "installation failed and rollback also failed");
        StringAssert.Contains(exception.Message, "Test-only post-install failure");
        StringAssert.Contains(exception.Message, "deliberate restore failure");
        Assert.IsTrue(Directory.Exists(Path.Combine(fixture.InstallParent, $"{install_name}.previous")));
        Assert.IsTrue(Directory.EnumerateDirectories(fixture.InstallParent, $"{install_name}.staging.*").Any());
    }

    private static AgentGitInstallerApplication CreateApplication(
        IProcessRunner? process_runner = null,
        IInstallerFileSystem? file_system = null)
    {
        return new AgentGitInstallerApplication(
            process_runner ?? new ProcessRunner(),
            new ExecutableLocator(),
            file_system ?? new InstallerFileSystem(),
            new Sha256ArtifactHasher(),
            TextWriter.Null);
    }

    private static Task InstallFromTestArtifactsAsync(TemporaryInstallerRepository fixture, string install_root)
    {
        return CreateApplication().InstallAsync(CreateRequest(fixture, install_root), CancellationToken.None);
    }

    private static InstallerRequest CreateRequest(TemporaryInstallerRepository fixture, string install_root)
    {
        return new InstallerRequest(
            FindSourceRoot(),
            fixture.RepositoryRoot,
            "dev",
            ".agent-git.json",
            install_root,
            null,
            true,
            FindRuntimeArtifacts(),
            false,
            false);
    }

    private static string FindRuntimeArtifacts()
    {
        var directory = Path.GetDirectoryName(typeof(AgentGit.Program).Assembly.Location)!;
        foreach (var file in new[]
                 {
                     "agent-git.exe",
                     "agent-git.dll",
                     "agent-git.deps.json",
                     "agent-git.runtimeconfig.json",
                     "GitSupport.dll",
                 })
        {
            Assert.IsTrue(File.Exists(Path.Combine(directory, file)), $"Missing test runtime artifact '{file}'.");
        }

        return directory;
    }

    private static string CreateValidationProject(TemporaryInstallerRepository fixture, bool test_succeeds)
    {
        var project_root = Directory.CreateDirectory(
            Path.Combine(fixture.InstallParent, $"validation-project-{Guid.NewGuid():N}")).FullName;
        var agent_git_project = SecurityElement.Escape(
            Path.Combine(FindSourceRoot(), "tools", "AgentGit", "AgentGit.csproj"));
        var project = $$"""
            <Project Sdk="Microsoft.NET.Sdk">
              <PropertyGroup>
                <TargetFramework>net10.0</TargetFramework>
                <ImplicitUsings>enable</ImplicitUsings>
                <Nullable>enable</Nullable>
                <IsPackable>false</IsPackable>
                <IsTestProject>true</IsTestProject>
              </PropertyGroup>
              <ItemGroup>
                <PackageReference Include="Microsoft.NET.Test.Sdk" Version="17.12.0" />
                <PackageReference Include="MSTest.TestAdapter" Version="3.6.4" />
                <PackageReference Include="MSTest.TestFramework" Version="3.6.4" />
                <ProjectReference Include="{{agent_git_project}}" />
              </ItemGroup>
            </Project>
            """;
        var project_path = Path.Combine(project_root, "InstallerValidation.csproj");
        File.WriteAllText(project_path, project);
        var assertion = test_succeeds
            ? "IsNotNull(typeof(object).Assembly)"
            : "Fail(\"deliberate gate failure\")";
        File.WriteAllText(
            Path.Combine(project_root, "ValidationTests.cs"),
            $$"""
            using Microsoft.VisualStudio.TestTools.UnitTesting;

            [TestClass]
            public sealed class ValidationTests
            {
                [TestMethod]
                public void Validated_build_loads()
                {
                    Assert.{{assertion}};
                }
            }
            """);
        return project_path;
    }

    private static void CreateDirectoryLink(string link, string target)
    {
        if (!OperatingSystem.IsWindows())
        {
            Directory.CreateSymbolicLink(link, target);
            return;
        }

        RunLinkCommand("mklink", "/J", link, target);
    }

    private static void RemoveDirectoryLink(string link)
    {
        if (!Directory.Exists(link))
        {
            return;
        }
        if (!OperatingSystem.IsWindows())
        {
            Directory.Delete(link);
            return;
        }

        RunLinkCommand("rmdir", link);
    }

    private static void RunLinkCommand(params string[] arguments)
    {
        var start_info = new ProcessStartInfo
        {
            FileName = "cmd.exe",
            UseShellExecute = false,
            RedirectStandardOutput = true,
            RedirectStandardError = true,
            CreateNoWindow = true,
        };
        start_info.ArgumentList.Add("/d");
        start_info.ArgumentList.Add("/c");
        foreach (var argument in arguments)
        {
            start_info.ArgumentList.Add(argument);
        }

        using var process = Process.Start(start_info) ?? throw new AssertFailedException("Unable to start link command.");
        var output = process.StandardOutput.ReadToEnd();
        var error = process.StandardError.ReadToEnd();
        process.WaitForExit();
        Assert.AreEqual(0, process.ExitCode, output + error);
    }

    private static JsonDocument LoadManifest(string install_root)
    {
        return JsonDocument.Parse(File.ReadAllText(Path.Combine(install_root, "trust.json")));
    }

    private static void AssertNoTransientDirectories(string parent, string install_name)
    {
        Assert.IsFalse(Directory.EnumerateDirectories(parent, $"{install_name}.staging.*").Any());
        Assert.IsFalse(Directory.EnumerateDirectories(parent, $"{install_name}.validation.*").Any());
    }

    private static string FindSourceRoot()
    {
        var configured = Environment.GetEnvironmentVariable("NUKETHEBEES_AGENT_GIT_SOURCE_ROOT");
        if (!string.IsNullOrWhiteSpace(configured))
        {
            return Path.GetFullPath(configured);
        }

        for (var directory = new DirectoryInfo(AppContext.BaseDirectory); directory is not null; directory = directory.Parent)
        {
            if (File.Exists(Path.Combine(directory.FullName, "dev.ps1")) &&
                File.Exists(Path.Combine(directory.FullName, "tools", "Tools.slnx")))
            {
                return directory.FullName;
            }
        }

        throw new AssertFailedException("Unable to locate repository source root.");
    }

    private sealed class GitVersionProcessRunner(IProcessRunner inner, string version) : IProcessRunner
    {
        public Task<ProcessResult> RunAsync(ProcessRequest request, CancellationToken cancellation_token)
        {
            if (request.Arguments is ["--version"] &&
                Path.GetFileName(request.FileName).StartsWith("git", StringComparison.OrdinalIgnoreCase))
            {
                return Task.FromResult(new ProcessResult(0, version, string.Empty));
            }

            return inner.RunAsync(request, cancellation_token);
        }
    }

    private sealed class SmokeFailureProcessRunner(IProcessRunner inner) : IProcessRunner
    {
        public Task<ProcessResult> RunAsync(ProcessRequest request, CancellationToken cancellation_token)
        {
            if (request.Arguments is ["--version"] &&
                Path.GetFileName(request.FileName).Equals("agent-git.exe", StringComparison.OrdinalIgnoreCase))
            {
                return Task.FromResult(new ProcessResult(1, string.Empty, "deliberate smoke failure"));
            }

            return inner.RunAsync(request, cancellation_token);
        }
    }

    private sealed class MoveBranchAfterResolutionProcessRunner(
        IProcessRunner inner,
        TemporaryInstallerRepository fixture,
        string moved_commit) : IProcessRunner
    {
        private bool moved;

        public async Task<ProcessResult> RunAsync(ProcessRequest request, CancellationToken cancellation_token)
        {
            var result = await inner.RunAsync(request, cancellation_token);
            var argument_count = request.Arguments.Count;
            if (!moved && argument_count >= 3 &&
                request.Arguments[argument_count - 3] == "rev-parse" &&
                request.Arguments[argument_count - 2] == "--verify" &&
                request.Arguments[argument_count - 1] == "refs/heads/dev^{commit}")
            {
                fixture.RunGit("update-ref", "refs/heads/dev", moved_commit);
                moved = true;
            }

            return result;
        }
    }

    private sealed class RestoreFailureFileSystem(IInstallerFileSystem inner) : IInstallerFileSystem
    {
        public bool FileExists(string path) => inner.FileExists(path);

        public bool DirectoryExists(string path) => inner.DirectoryExists(path);

        public FileAttributes GetAttributes(string path) => inner.GetAttributes(path);

        public long FileLength(string path) => inner.FileLength(path);

        public string ReadAllText(string path) => inner.ReadAllText(path);

        public void WriteAllText(string path, string contents) => inner.WriteAllText(path, contents);

        public void AppendAllText(string path, string contents) => inner.AppendAllText(path, contents);

        public void CreateDirectory(string path) => inner.CreateDirectory(path);

        public void CopyFile(string source, string destination) => inner.CopyFile(source, destination);

        public void MoveDirectory(string source, string destination)
        {
            if (source.EndsWith(".previous", StringComparison.OrdinalIgnoreCase))
            {
                throw new IOException("deliberate restore failure");
            }

            inner.MoveDirectory(source, destination);
        }

        public void DeleteDirectory(string path) => inner.DeleteDirectory(path);
    }
}
