using System.Collections;
using System.Text.Json;
using System.Text.Json.Serialization;
using System.Text.RegularExpressions;
using AgentGit;

namespace AgentGitInstaller;

internal sealed class AgentGitInstallerApplication(
    IProcessRunner process_runner,
    IExecutableLocator executable_locator,
    IInstallerFileSystem file_system,
    IArtifactHasher artifact_hasher,
    TextWriter output)
{
    private static readonly string[] runtime_files =
    [
        "agent-git.exe",
        "agent-git.dll",
        "agent-git.deps.json",
        "agent-git.runtimeconfig.json",
        "GitSupport.dll",
    ];

    private static readonly JsonSerializerOptions trust_json_options = new()
    {
        PropertyNamingPolicy = JsonNamingPolicy.CamelCase,
        PropertyNameCaseInsensitive = false,
        UnmappedMemberHandling = JsonUnmappedMemberHandling.Disallow,
        WriteIndented = true,
    };

    public async Task InstallAsync(InstallerRequest request, CancellationToken cancellation_token)
    {
        ArgumentNullException.ThrowIfNull(request);

        var source_root = PathSafety.ExistingDirectory(request.SourceRoot, "Source root", file_system);
        var repository_root = PathSafety.ExistingDirectory(request.Repository, "Repository root", file_system);
        ValidateSourceRoot(source_root);
        PathSafety.EnsureNoReparsePoints(source_root, "Source root", file_system);
        PathSafety.EnsureNoReparsePoints(repository_root, "Repository root", file_system);
        PathSafety.ValidatePolicyPath(request.PolicyPath);

        var git_path = executable_locator.Find(OperatingSystem.IsWindows() ? "git.exe" : "git");
        PathSafety.EnsureNoReparsePoints(git_path, "Trusted Git executable path", file_system);
        var git_environment = CreateGitInspectionEnvironment();
        await ValidateGitVersionAsync(git_path, repository_root, git_environment, cancellation_token);
        var repository = await InspectRepositoryAsync(
            git_path,
            repository_root,
            request.BaseBranch,
            request.PolicyPath,
            git_environment,
            cancellation_token);

        var local_app_data = Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData);
        if (string.IsNullOrWhiteSpace(local_app_data))
        {
            throw new InstallerException("Unable to locate the current user's local application data directory.");
        }

        var canonical_root = Path.GetFullPath(Path.Combine(local_app_data, "NukeTheBees", "agent-git"));
        var install_root = Path.GetFullPath(request.InstallRoot ?? canonical_root);
        PathSafety.EnsureSafeInstallRoot(install_root);
        var install_parent = Path.GetDirectoryName(install_root)
            ?? throw new InstallerException($"Unable to determine the installation parent for '{install_root}'.");
        var install_name = Path.GetFileName(Path.TrimEndingDirectorySeparator(install_root));
        var canonical_install = PathSafety.PathsEqual(install_root, canonical_root);
        ValidateTestControls(request, install_root, canonical_root, canonical_install);

        PathSafety.EnsureNoReparsePoints(install_parent, "Installation path", file_system);
        if (file_system.FileExists(install_root))
        {
            throw new InstallerException($"Installation root is a file: '{install_root}'.");
        }
        PathSafety.EnsureNoReparsePoints(install_root, "Installation root", file_system);
        file_system.CreateDirectory(install_parent);
        PathSafety.EnsureNoReparsePoints(install_parent, "Installation path", file_system);

        string? git_lfs_path = null;
        if (repository.Policy.GitExtensions.Contains("lfs"))
        {
            git_lfs_path = executable_locator.Find(OperatingSystem.IsWindows() ? "git-lfs.exe" : "git-lfs");
            PathSafety.EnsureNoReparsePoints(git_lfs_path, "Trusted Git LFS executable path", file_system);
        }

        var operation_id = Guid.NewGuid().ToString("N");
        var validation_root = Path.Combine(install_parent, $"{install_name}.validation.{operation_id}");
        var staging_root = Path.Combine(install_parent, $"{install_name}.staging.{operation_id}");
        var previous_root = Path.Combine(install_parent, $"{install_name}.previous");
        var moved_previous_install = false;
        var activated_new_install = false;
        var rollback_failed = false;
        Exception? failure = null;

        try
        {
            var artifacts = await PrepareArtifactsAsync(
                request,
                source_root,
                validation_root,
                cancellation_token);
            if (artifacts.ValidationRoot is null)
            {
                validation_root = string.Empty;
            }

            VerifyArtifactHashes(artifacts.Root, artifacts.Hashes);
            StageInstallation(
                staging_root,
                artifacts.Root,
                artifacts.Hashes,
                install_root,
                git_path,
                git_lfs_path,
                repository);

            ValidatePreviousRoot(previous_root);
            if (file_system.DirectoryExists(previous_root))
            {
                file_system.DeleteDirectory(previous_root);
            }

            if (file_system.DirectoryExists(install_root))
            {
                file_system.MoveDirectory(install_root, previous_root);
                moved_previous_install = true;
            }

            file_system.MoveDirectory(staging_root, install_root);
            activated_new_install = true;

            VerifyArtifactHashes(Path.Combine(install_root, "bin"), artifacts.Hashes);
            var installed_executable = Path.Combine(install_root, "bin", "agent-git.exe");
            await RequireSuccessAsync(
                new ProcessRequest(installed_executable, ["--version"], install_root),
                "The installed agent-git executable failed its version smoke test.",
                cancellation_token);

            if (request.TestFailPostInstall)
            {
                throw new InstallerException("Test-only post-install failure.");
            }

            if (canonical_install)
            {
                await RequireSuccessAsync(
                    new ProcessRequest(installed_executable, ["status"], repository_root),
                    "The installed agent-git executable failed its trusted repository smoke test.",
                    cancellation_token);
            }

            if (artifacts.ValidationRoot is not null && file_system.DirectoryExists(artifacts.ValidationRoot))
            {
                file_system.DeleteDirectory(artifacts.ValidationRoot);
                validation_root = string.Empty;
            }
        }
        catch (Exception exception)
        {
            failure = exception;
            try
            {
                if (activated_new_install && file_system.DirectoryExists(install_root))
                {
                    file_system.MoveDirectory(install_root, staging_root);
                    activated_new_install = false;
                }

                if (moved_previous_install && file_system.DirectoryExists(previous_root))
                {
                    file_system.MoveDirectory(previous_root, install_root);
                    moved_previous_install = false;
                }
            }
            catch (Exception rollback_exception)
            {
                rollback_failed = true;
                failure = new InstallerException(
                    "agent-git installation failed and rollback also failed. " +
                    $"The previous installation may remain at '{previous_root}'. " +
                    $"Installation failure: {exception.Message} Rollback failure: {rollback_exception.Message}",
                    exception);
            }
        }

        if (!rollback_failed)
        {
            failure = CleanupPath(staging_root, "staging", failure);
        }
        failure = CleanupPath(validation_root, "validation", failure);
        if (failure is not null)
        {
            throw failure;
        }

        await output.WriteLineAsync($"Installed agent-git at '{Path.Combine(install_root, "bin", "agent-git.exe")}'.");
        await output.WriteLineAsync(
            $"Registered repository '{repository.Policy.RepositoryId}' with policy ref '{repository.PolicyRef}'.");
    }

    private void ValidateSourceRoot(string source_root)
    {
        foreach (var relative_path in new[]
                 {
                     "dev.ps1",
                     Path.Combine("tools", "Tools.slnx"),
                     Path.Combine("tools", "AgentGit", "AgentGit.csproj"),
                     Path.Combine("tools", "AgentGit.Tests", "AgentGit.Tests.csproj"),
                 })
        {
            var path = Path.Combine(source_root, relative_path);
            if (!file_system.FileExists(path))
            {
                throw new InstallerException($"Source root is missing required file '{path}'.");
            }
        }
    }

    private static void ValidateTestControls(
        InstallerRequest request,
        string install_root,
        string canonical_root,
        bool canonical_install)
    {
        var has_test_controls = request.TestSkipValidation || request.TestArtifactRoot is not null ||
            request.TestValidationProject is not null || request.TestCorruptValidatedArtifact ||
            request.TestFailPostInstall;
        if (canonical_install && has_test_controls)
        {
            throw new InstallerException(
                "Canonical installation cannot skip or override the AgentGit validation project.");
        }

        if (has_test_controls && !PathSafety.IsStrictlyWithin(Path.GetTempPath(), install_root))
        {
            throw new InstallerException(
                "Non-canonical validation controls are restricted to test installations under the system temporary directory.");
        }

        if (request.TestSkipValidation != (request.TestArtifactRoot is not null))
        {
            throw new InstallerException(
                "--test-skip-validation and --test-artifact-root must be specified together.");
        }

        if (request.TestSkipValidation && request.TestCorruptValidatedArtifact)
        {
            throw new InstallerException("--test-corrupt-validated-artifact requires a validation build.");
        }

        if (!canonical_install && PathSafety.PathsEqual(install_root, canonical_root))
        {
            throw new InstallerException("Canonical installation path comparison was inconsistent.");
        }
    }

    private async Task ValidateGitVersionAsync(
        string git_path,
        string repository_root,
        IReadOnlyDictionary<string, string?> git_environment,
        CancellationToken cancellation_token)
    {
        var result = await process_runner.RunAsync(
            new ProcessRequest(
                git_path,
                ["--version"],
                repository_root,
                git_environment,
                TimeSpan.FromSeconds(30)),
            cancellation_token);
        var match = Regex.Match(
            result.StandardOutput.Trim(),
            "^git version (?<major>\\d+)\\.(?<minor>\\d+)",
            RegexOptions.CultureInvariant);
        if (result.ExitCode != 0 || !match.Success ||
            !int.TryParse(match.Groups["major"].Value, out var major) ||
            !int.TryParse(match.Groups["minor"].Value, out var minor))
        {
            throw new InstallerException($"Unable to determine the trusted Git version from '{git_path}'.");
        }

        if (new Version(major, minor) < new Version(2, 38))
        {
            throw new InstallerException(
                $"agent-git requires Git 2.38 or newer; found {result.StandardOutput.Trim()}.");
        }
    }

    private async Task<RepositoryInstallation> InspectRepositoryAsync(
        string git_path,
        string repository_root,
        string base_branch,
        string policy_path,
        IReadOnlyDictionary<string, string?> git_environment,
        CancellationToken cancellation_token)
    {
        var validated_branch = await RunGitTextAsync(
            git_path,
            repository_root,
            ["check-ref-format", "--branch", base_branch],
            git_environment,
            cancellation_token);
        if (!string.Equals(validated_branch, base_branch, StringComparison.Ordinal))
        {
            throw new InstallerException($"Base branch must be a direct local branch name; found '{base_branch}'.");
        }

        var policy_ref = $"refs/heads/{base_branch}";
        var policy_commit = await RunGitTextAsync(
            git_path,
            repository_root,
            ["rev-parse", "--verify", $"{policy_ref}^{{commit}}"],
            git_environment,
            cancellation_token);
        var common_git_directory = Path.GetFullPath(await RunGitTextAsync(
            git_path,
            repository_root,
            ["rev-parse", "--path-format=absolute", "--git-common-dir"],
            git_environment,
            cancellation_token));
        if (!file_system.DirectoryExists(common_git_directory))
        {
            throw new InstallerException(
                $"Git reported a common Git directory that does not exist: '{common_git_directory}'.");
        }
        PathSafety.EnsureNoReparsePoints(common_git_directory, "Common Git directory", file_system);

        var origin_url = await RunGitTextAsync(
            git_path,
            repository_root,
            ["config", "--local", "--get", "remote.origin.url"],
            git_environment,
            cancellation_token);
        var user_name = await RunGitTextAsync(
            git_path,
            repository_root,
            ["config", "--get", "user.name"],
            git_environment,
            cancellation_token);
        var user_email = await RunGitTextAsync(
            git_path,
            repository_root,
            ["config", "--get", "user.email"],
            git_environment,
            cancellation_token);
        var policy_object = $"{policy_commit}:{policy_path}";
        var policy_size_text = await RunGitTextAsync(
            git_path,
            repository_root,
            ["cat-file", "-s", policy_object],
            git_environment,
            cancellation_token);
        if (!long.TryParse(policy_size_text, out var policy_size) || policy_size is < 0 or > 64 * 1024)
        {
            throw new InstallerException(
                $"Policy document has invalid size '{policy_size_text}' or exceeds the 64 KiB safety limit.");
        }

        var policy_json = await RunGitTextAsync(
            git_path,
            repository_root,
            ["cat-file", "blob", policy_object],
            git_environment,
            cancellation_token);
        var policy = PolicyLoader.Parse(policy_json);
        if (!string.Equals(policy.BaseBranch, base_branch, StringComparison.Ordinal))
        {
            throw new InstallerException(
                $"Policy baseBranch '{policy.BaseBranch}' does not match requested base '{base_branch}'.");
        }

        return new RepositoryInstallation(
            common_git_directory,
            origin_url,
            user_name,
            user_email,
            policy_ref,
            policy_path,
            policy);
    }

    private async Task<string> RunGitTextAsync(
        string git_path,
        string repository_root,
        IReadOnlyList<string> arguments,
        IReadOnlyDictionary<string, string?> git_environment,
        CancellationToken cancellation_token)
    {
        var prefixed_arguments = new[] { "-C", repository_root }.Concat(arguments).ToArray();
        var result = await process_runner.RunAsync(
            new ProcessRequest(
                git_path,
                prefixed_arguments,
                repository_root,
                git_environment,
                TimeSpan.FromSeconds(30)),
            cancellation_token);
        if (result.ExitCode != 0)
        {
            var diagnostic = string.IsNullOrWhiteSpace(result.StandardError)
                ? $"exit code {result.ExitCode}"
                : result.StandardError.Trim();
            throw new InstallerException(
                $"Git command failed: git {string.Join(' ', arguments)}: {diagnostic}");
        }

        return result.StandardOutput.Trim();
    }

    private static IReadOnlyDictionary<string, string?> CreateGitInspectionEnvironment()
    {
        var environment = new Dictionary<string, string?>(StringComparer.OrdinalIgnoreCase);
        foreach (DictionaryEntry entry in Environment.GetEnvironmentVariables())
        {
            if (entry.Key is string name && name.StartsWith("GIT_", StringComparison.OrdinalIgnoreCase))
            {
                environment[name] = null;
            }
        }

        var user_profile = Environment.GetFolderPath(Environment.SpecialFolder.UserProfile);
        if (!string.IsNullOrWhiteSpace(user_profile))
        {
            environment["USERPROFILE"] = user_profile;
            environment["HOME"] = user_profile;
            environment["XDG_CONFIG_HOME"] = null;
        }

        environment["LC_ALL"] = "C";
        environment["LANG"] = "C";
        environment["GIT_CONFIG_NOSYSTEM"] = "1";
        environment["GIT_NO_REPLACE_OBJECTS"] = "1";
        environment["GIT_OPTIONAL_LOCKS"] = "0";
        environment["GIT_TERMINAL_PROMPT"] = "0";
        environment["GCM_INTERACTIVE"] = "Never";
        return environment;
    }

    private async Task<PreparedArtifacts> PrepareArtifactsAsync(
        InstallerRequest request,
        string source_root,
        string validation_root,
        CancellationToken cancellation_token)
    {
        if (request.TestSkipValidation)
        {
            await output.WriteLineAsync("Skipping AgentGit validation for a non-canonical test installation.");
            var test_artifact_root = Path.GetFullPath(request.TestArtifactRoot!);
            PathSafety.EnsureNoReparsePoints(test_artifact_root, "Test artifact path", file_system);
            return new PreparedArtifacts(test_artifact_root, HashArtifacts(test_artifact_root), null);
        }

        var validation_project = request.TestValidationProject is null
            ? Path.Combine(source_root, "tools", "AgentGit.Tests", "AgentGit.Tests.csproj")
            : Path.GetFullPath(request.TestValidationProject);
        if (!file_system.FileExists(validation_project))
        {
            throw new InstallerException($"AgentGit validation project was not found at '{validation_project}'.");
        }
        PathSafety.EnsureNoReparsePoints(validation_project, "Validation project path", file_system);

        PathSafety.EnsureNoReparsePoints(validation_root, "Validation path", file_system);
        var private_artifacts = Path.Combine(validation_root, "artifacts");
        var artifact_root = Path.Combine(validation_root, "runtime");
        file_system.CreateDirectory(private_artifacts);
        file_system.CreateDirectory(artifact_root);
        PathSafety.EnsureNoReparsePoints(validation_root, "Validation path", file_system);

        var dotnet_path = executable_locator.Find(OperatingSystem.IsWindows() ? "dotnet.exe" : "dotnet");
        PathSafety.EnsureNoReparsePoints(dotnet_path, "Trusted .NET executable path", file_system);
        var stage_property = $"-p:StandaloneToolsBinDirectory={artifact_root}{Path.DirectorySeparatorChar}";
        await output.WriteLineAsync("Building trusted AgentGit source into a private installation directory.");
        var build = await process_runner.RunAsync(
            new ProcessRequest(
                dotnet_path,
                [
                    "build", validation_project,
                    "--artifacts-path", private_artifacts,
                    stage_property,
                    "-m:1", "-nr:false",
                ],
                source_root),
            cancellation_token);
        WriteProcessOutput(build);
        if (build.ExitCode != 0)
        {
            throw new InstallerException(
                $"AgentGit validation build failed with exit code {build.ExitCode}; installation was not activated.");
        }

        var hashes = HashArtifacts(artifact_root);
        await output.WriteLineAsync("Running the mandatory AgentGit security test gate against the private build.");
        var test = await process_runner.RunAsync(
            new ProcessRequest(
                dotnet_path,
                [
                    "test", validation_project,
                    "--no-build", "--no-restore",
                    "--artifacts-path", private_artifacts,
                    stage_property,
                    "-m:1", "-nr:false",
                ],
                source_root,
                new Dictionary<string, string?>
                {
                    ["NUKETHEBEES_AGENT_GIT_SOURCE_ROOT"] = source_root,
                }),
            cancellation_token);
        WriteProcessOutput(test);
        if (test.ExitCode != 0)
        {
            throw new InstallerException(
                $"AgentGit security test gate failed with exit code {test.ExitCode}; installation was not activated.");
        }

        if (request.TestCorruptValidatedArtifact)
        {
            file_system.AppendAllText(Path.Combine(artifact_root, "agent-git.dll"), "test-only-corruption");
        }

        VerifyArtifactHashes(artifact_root, hashes);
        foreach (var file in runtime_files)
        {
            await output.WriteLineAsync($"Validated private artifact SHA256 {file}={hashes[file]}");
        }

        return new PreparedArtifacts(artifact_root, hashes, validation_root);
    }

    private Dictionary<string, string> HashArtifacts(string artifact_root)
    {
        if (!file_system.DirectoryExists(artifact_root))
        {
            throw new InstallerException($"AgentGit runtime artifact directory does not exist: '{artifact_root}'.");
        }
        PathSafety.EnsureNoReparsePoints(artifact_root, "AgentGit runtime artifact path", file_system);

        var hashes = new Dictionary<string, string>(StringComparer.Ordinal);
        foreach (var file in runtime_files)
        {
            var path = Path.Combine(artifact_root, file);
            if (!file_system.FileExists(path))
            {
                throw new InstallerException($"Required AgentGit runtime artifact is missing: '{path}'.");
            }
            if ((file_system.GetAttributes(path) & FileAttributes.ReparsePoint) != 0)
            {
                throw new InstallerException(
                    $"AgentGit runtime artifact cannot be a symbolic link or reparse point: '{path}'.");
            }

            hashes.Add(file, artifact_hasher.Hash(path));
        }

        return hashes;
    }

    private void VerifyArtifactHashes(string artifact_root, IReadOnlyDictionary<string, string> expected_hashes)
    {
        var actual_hashes = HashArtifacts(artifact_root);
        foreach (var file in runtime_files)
        {
            if (!string.Equals(expected_hashes[file], actual_hashes[file], StringComparison.OrdinalIgnoreCase))
            {
                throw new InstallerException(
                    $"Validated AgentGit runtime artifact changed after validation: '{file}'.");
            }
        }
    }

    private void StageInstallation(
        string staging_root,
        string artifact_root,
        IReadOnlyDictionary<string, string> artifact_hashes,
        string install_root,
        string git_path,
        string? git_lfs_path,
        RepositoryInstallation repository)
    {
        PathSafety.EnsureNoReparsePoints(staging_root, "Staging path", file_system);
        var staging_bin = Path.Combine(staging_root, "bin");
        var staging_config = Path.Combine(staging_root, "config");
        file_system.CreateDirectory(staging_bin);
        file_system.CreateDirectory(staging_config);
        PathSafety.EnsureNoReparsePoints(staging_root, "Staging path", file_system);
        file_system.WriteAllText(Path.Combine(staging_config, "empty-hooks"), string.Empty);
        file_system.WriteAllText(Path.Combine(staging_config, "empty.gitconfig"), string.Empty);
        file_system.WriteAllText(Path.Combine(staging_config, "empty.attributes"), string.Empty);

        VerifyArtifactHashes(artifact_root, artifact_hashes);
        foreach (var file in runtime_files)
        {
            file_system.CopyFile(Path.Combine(artifact_root, file), Path.Combine(staging_bin, file));
        }
        VerifyArtifactHashes(staging_bin, artifact_hashes);

        var existing_manifest = LoadExistingManifest(install_root);
        var repositories = existing_manifest?.Repositories
            .Where(item => !PathSafety.PathsEqual(item.CommonGitDirectory, repository.CommonGitDirectory))
            .ToList() ?? [];
        var manifest_settings = ResolveManifestSettings(
            existing_manifest,
            repositories.Count != 0,
            git_path,
            git_lfs_path,
            repository.UserName,
            repository.UserEmail);

        repositories.Add(new TrustedRepository
        {
            RepositoryId = repository.Policy.RepositoryId,
            CommonGitDirectory = repository.CommonGitDirectory,
            OriginUrl = repository.OriginUrl,
            PolicyRef = repository.PolicyRef,
            PolicyPath = repository.PolicyPath,
        });
        var manifest = new TrustManifest
        {
            Version = 1,
            GitExecutable = manifest_settings.GitExecutable,
            GitLfsExecutable = manifest_settings.GitLfsExecutable,
            UserName = manifest_settings.UserName,
            UserEmail = manifest_settings.UserEmail,
            Repositories = repositories.ToArray(),
        };
        TrustStore.ValidateManifest(manifest);
        file_system.WriteAllText(
            Path.Combine(staging_root, "trust.json"),
            JsonSerializer.Serialize(manifest, trust_json_options));
    }

    private static ManifestSettings ResolveManifestSettings(
        TrustManifest? existing_manifest,
        bool preserves_other_repositories,
        string git_path,
        string? git_lfs_path,
        string user_name,
        string user_email)
    {
        if (!preserves_other_repositories)
        {
            return new ManifestSettings(git_path, git_lfs_path, user_name, user_email);
        }

        var existing = existing_manifest
            ?? throw new InstallerException("Cannot preserve repository registrations without an existing trust manifest.");
        if (!PathSafety.PathsEqual(existing.GitExecutable, git_path))
        {
            throw new InstallerException(
                "Cannot preserve existing repository registrations because the trusted Git executable differs. " +
                $"Existing: '{existing.GitExecutable}'. Requested: '{git_path}'.");
        }

        if (existing.GitLfsExecutable is null != (git_lfs_path is null))
        {
            throw new InstallerException(
                "Cannot preserve existing repository registrations because the trusted Git LFS mode differs.");
        }
        if (existing.GitLfsExecutable is not null &&
            !PathSafety.PathsEqual(existing.GitLfsExecutable, git_lfs_path!))
        {
            throw new InstallerException(
                "Cannot preserve existing repository registrations because the trusted Git LFS executable differs. " +
                $"Existing: '{existing.GitLfsExecutable}'. Requested: '{git_lfs_path}'.");
        }

        if (!string.Equals(existing.UserName, user_name, StringComparison.Ordinal))
        {
            throw new InstallerException(
                "Cannot preserve existing repository registrations because the trusted Git user name differs. " +
                $"Existing: '{existing.UserName}'. Requested: '{user_name}'.");
        }
        if (!string.Equals(existing.UserEmail, user_email, StringComparison.Ordinal))
        {
            throw new InstallerException(
                "Cannot preserve existing repository registrations because the trusted Git user email differs. " +
                $"Existing: '{existing.UserEmail}'. Requested: '{user_email}'.");
        }

        return new ManifestSettings(
            existing.GitExecutable,
            existing.GitLfsExecutable,
            existing.UserName,
            existing.UserEmail);
    }

    private TrustManifest? LoadExistingManifest(string install_root)
    {
        var manifest_path = Path.Combine(install_root, "trust.json");
        if (!file_system.FileExists(manifest_path))
        {
            return null;
        }
        if ((file_system.GetAttributes(manifest_path) & FileAttributes.ReparsePoint) != 0)
        {
            throw new InstallerException(
                $"Existing trust manifest cannot be a symbolic link or reparse point: '{manifest_path}'.");
        }
        if (file_system.FileLength(manifest_path) > 64 * 1024)
        {
            throw new InstallerException("Existing trust manifest exceeds the 64 KiB safety limit.");
        }

        var manifest = TrustStore.ParseManifest(file_system.ReadAllText(manifest_path));
        if (manifest.Version != 1)
        {
            throw new InstallerException(
                $"Unsupported existing trust manifest version {manifest.Version}; expected version 1.");
        }
        TrustStore.ValidateManifest(manifest);

        return manifest;
    }

    private void ValidatePreviousRoot(string previous_root)
    {
        if (file_system.FileExists(previous_root))
        {
            throw new InstallerException($"Previous installation path is a file: '{previous_root}'.");
        }
        PathSafety.EnsureNoReparsePoints(previous_root, "Previous installation path", file_system);
    }

    private async Task RequireSuccessAsync(
        ProcessRequest request,
        string failure_message,
        CancellationToken cancellation_token)
    {
        var result = await process_runner.RunAsync(request, cancellation_token);
        WriteProcessOutput(result);
        if (result.ExitCode != 0)
        {
            var diagnostic = string.IsNullOrWhiteSpace(result.StandardError)
                ? string.Empty
                : $" {result.StandardError.Trim()}";
            throw new InstallerException($"{failure_message}{diagnostic}");
        }
    }

    private void WriteProcessOutput(ProcessResult result)
    {
        if (!string.IsNullOrEmpty(result.StandardOutput))
        {
            output.Write(result.StandardOutput);
        }
        if (!string.IsNullOrEmpty(result.StandardError))
        {
            output.Write(result.StandardError);
        }
    }

    private Exception? CleanupPath(string path, string description, Exception? failure)
    {
        if (string.IsNullOrWhiteSpace(path) || !file_system.DirectoryExists(path))
        {
            return failure;
        }

        try
        {
            file_system.DeleteDirectory(path);
            return failure;
        }
        catch (Exception cleanup_exception)
        {
            if (failure is null)
            {
                return new InstallerException(
                    $"AgentGit installation completed but {description} cleanup failed for '{path}': " +
                    cleanup_exception.Message,
                    cleanup_exception);
            }

            return new InstallerException(
                $"{failure.Message} Additionally, {description} cleanup failed for '{path}': " +
                cleanup_exception.Message,
                failure);
        }
    }

    private sealed record RepositoryInstallation(
        string CommonGitDirectory,
        string OriginUrl,
        string UserName,
        string UserEmail,
        string PolicyRef,
        string PolicyPath,
        GitPolicy Policy);

    private sealed record PreparedArtifacts(
        string Root,
        IReadOnlyDictionary<string, string> Hashes,
        string? ValidationRoot);

    private sealed record ManifestSettings(
        string GitExecutable,
        string? GitLfsExecutable,
        string UserName,
        string UserEmail);
}
