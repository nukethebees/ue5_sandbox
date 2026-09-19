using System.Text.Json;
using System.Text.Json.Serialization;

namespace AgentGit;

internal sealed class TrustManifest
{
    public int Version { get; init; }

    public required string GitExecutable { get; init; }

    public string? GitLfsExecutable { get; init; }

    public required string UserName { get; init; }

    public required string UserEmail { get; init; }

    public required TrustedRepository[] Repositories { get; init; }
}

internal sealed class TrustedRepository
{
    public required string RepositoryId { get; init; }

    public required string CommonGitDirectory { get; init; }

    public required string OriginUrl { get; init; }

    public required string PolicyRef { get; init; }

    public required string PolicyPath { get; init; }
}

internal sealed record TrustContext(
    string InstallationRoot,
    string GitExecutable,
    string? GitLfsExecutable,
    string UserName,
    string UserEmail,
    string EmptyConfigPath,
    string EmptyHooksDirectory,
    IReadOnlyList<TrustedRepository> Repositories)
{
    public TrustedRepository FindRegistration(string common_git_directory)
    {
        var matches = Repositories.Where(repository => PathsEqual(
            repository.CommonGitDirectory,
            common_git_directory)).ToArray();
        return matches.Length switch
        {
            1 => matches[0],
            0 => throw new RepositoryException(
                $"Repository common Git directory '{common_git_directory}' is not registered with agent-git."),
            _ => throw new PolicyConfigurationException(
                $"Trust manifest contains duplicate registrations for '{common_git_directory}'."),
        };
    }

    private static bool PathsEqual(string left, string right)
    {
        return string.Equals(
            Path.GetFullPath(left).TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar),
            Path.GetFullPath(right).TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar),
            OperatingSystem.IsWindows() ? StringComparison.OrdinalIgnoreCase : StringComparison.Ordinal);
    }
}

internal static class TrustStore
{
    private static readonly JsonSerializerOptions json_options = new()
    {
        PropertyNamingPolicy = JsonNamingPolicy.CamelCase,
        PropertyNameCaseInsensitive = false,
        UnmappedMemberHandling = JsonUnmappedMemberHandling.Disallow,
    };

    public static TrustContext LoadCanonical(string process_path)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(process_path);

        var local_app_data = Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData);
        if (string.IsNullOrWhiteSpace(local_app_data))
        {
            throw new PolicyConfigurationException("Unable to locate the current user's local application data directory.");
        }

        var installation_root = Path.GetFullPath(Path.Combine(local_app_data, "NukeTheBees", "agent-git"));
        var expected_executable = Path.Combine(installation_root, "bin", "agent-git.exe");
        if (!PathsEqual(process_path, expected_executable))
        {
            throw new PolicyConfigurationException(
                $"Mutating agent-git operations require the canonical installation at '{expected_executable}'. " +
                "Repository build outputs are not trusted executables.");
        }

        var manifest_path = Path.Combine(installation_root, "trust.json");
        string manifest_json;
        try
        {
            if (new FileInfo(manifest_path).Length > 64 * 1024)
            {
                throw new PolicyConfigurationException("Trust manifest exceeds the 64 KiB safety limit.");
            }

            manifest_json = File.ReadAllText(manifest_path);
        }
        catch (Exception exception) when (exception is IOException or UnauthorizedAccessException)
        {
            throw new PolicyConfigurationException(
                $"Unable to load agent-git trust manifest '{manifest_path}': {exception.Message}", exception);
        }

        var manifest = ParseManifest(manifest_json);
        ValidateManifest(manifest);

        var isolation = ValidateIsolationLayout(installation_root);

        return new TrustContext(
            installation_root,
            Path.GetFullPath(manifest.GitExecutable),
            manifest.GitLfsExecutable is null ? null : Path.GetFullPath(manifest.GitLfsExecutable),
            manifest.UserName,
            manifest.UserEmail,
            isolation.EmptyConfig,
            isolation.EmptyHooks,
            manifest.Repositories);
    }

    internal static TrustManifest ParseManifest(string json)
    {
        try
        {
            StrictJson.RejectDuplicateProperties(json);
            return JsonSerializer.Deserialize<TrustManifest>(json, json_options)
                ?? throw new PolicyConfigurationException("Trust manifest contained null.");
        }
        catch (JsonException exception)
        {
            throw new PolicyConfigurationException($"Trust manifest JSON is malformed: {exception.Message}", exception);
        }
    }

    internal static (string EmptyConfig, string EmptyHooks) ValidateIsolationLayout(string installation_root)
    {
        var bin = Path.Combine(installation_root, "bin");
        var config = Path.Combine(installation_root, "config");
        var empty_config = Path.Combine(installation_root, "config", "empty.gitconfig");
        var empty_hooks = Path.Combine(installation_root, "config", "empty-hooks");
        var empty_attributes = Path.Combine(installation_root, "config", "empty.attributes");
        if (!Directory.Exists(installation_root) || !Directory.Exists(bin) || !Directory.Exists(config) ||
            IsReparsePoint(installation_root) || IsReparsePoint(bin) || IsReparsePoint(config) ||
            !File.Exists(empty_config) || new FileInfo(empty_config).Length != 0 ||
            !File.Exists(empty_attributes) || new FileInfo(empty_attributes).Length != 0 ||
            !Directory.Exists(empty_hooks) || Directory.EnumerateFileSystemEntries(empty_hooks).Any() ||
            IsReparsePoint(empty_config) || IsReparsePoint(empty_attributes) || IsReparsePoint(empty_hooks))
        {
            throw new PolicyConfigurationException(
                "The canonical empty Git configuration or hook directory is missing or invalid.");
        }

        return (empty_config, empty_hooks);
    }

    internal static void ValidateManifest(TrustManifest manifest)
    {
        ArgumentNullException.ThrowIfNull(manifest);

        if (manifest.Version != 1)
        {
            throw new PolicyConfigurationException(
                $"Unsupported trust manifest version {manifest.Version}; expected version 1.");
        }

        ValidateFile(manifest.GitExecutable, "Git executable");
        if (manifest.GitLfsExecutable is not null)
        {
            ValidateFile(manifest.GitLfsExecutable, "Git LFS executable");
        }

        ValidateText(manifest.UserName, "Git user name");
        ValidateText(manifest.UserEmail, "Git user email");
        if (manifest.Repositories is null || manifest.Repositories.Length == 0)
        {
            throw new PolicyConfigurationException("Trust manifest does not register any repositories.");
        }

        var common_directories = new HashSet<string>(
            OperatingSystem.IsWindows() ? StringComparer.OrdinalIgnoreCase : StringComparer.Ordinal);
        foreach (var repository in manifest.Repositories)
        {
            if (repository is null)
            {
                throw new PolicyConfigurationException("Trust manifest contains a null repository registration.");
            }

            ValidateText(repository.RepositoryId, "Repository identity");
            ValidateText(repository.OriginUrl, "Repository origin");
            if (string.IsNullOrWhiteSpace(repository.CommonGitDirectory) ||
                !Path.IsPathFullyQualified(repository.CommonGitDirectory) ||
                !Directory.Exists(repository.CommonGitDirectory))
            {
                throw new PolicyConfigurationException(
                    $"Registered common Git directory is not an existing absolute directory: " +
                    $"'{repository.CommonGitDirectory}'.");
            }

            var common_directory = Path.GetFullPath(repository.CommonGitDirectory)
                .TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar);
            if (IsReparsePoint(common_directory))
            {
                throw new PolicyConfigurationException(
                    $"Registered common Git directory cannot be a symbolic link or reparse point: " +
                    $"'{repository.CommonGitDirectory}'.");
            }

            if (!common_directories.Add(common_directory))
            {
                throw new PolicyConfigurationException(
                    $"Trust manifest contains duplicate registrations for '{common_directory}'.");
            }

            if (!IsValidPolicyRef(repository.PolicyRef))
            {
                throw new PolicyConfigurationException(
                    $"Registered policy ref is not a direct local branch ref: '{repository.PolicyRef}'.");
            }

            if (!IsValidPolicyPath(repository.PolicyPath))
            {
                throw new PolicyConfigurationException(
                    $"Registered policy path is not a safe repository-relative Git path: '{repository.PolicyPath}'.");
            }
        }
    }

    private static void ValidateFile(string path, string description)
    {
        if (string.IsNullOrWhiteSpace(path) || !Path.IsPathFullyQualified(path) || !File.Exists(path))
        {
            throw new PolicyConfigurationException($"{description} is not an existing absolute file: '{path}'.");
        }

        if (IsReparsePoint(path))
        {
            throw new PolicyConfigurationException(
                $"{description} cannot be a symbolic link or reparse point: '{path}'.");
        }
    }

    private static void ValidateText(string value, string description)
    {
        if (string.IsNullOrWhiteSpace(value) || value.Any(char.IsControl))
        {
            throw new PolicyConfigurationException(
                $"{description} must be non-empty and cannot contain control characters.");
        }
    }

    private static bool IsReparsePoint(string path)
    {
        return (File.GetAttributes(path) & FileAttributes.ReparsePoint) != 0;
    }

    private static bool IsValidPolicyRef(string value)
    {
        const string prefix = "refs/heads/";
        if (string.IsNullOrWhiteSpace(value) || !value.StartsWith(prefix, StringComparison.Ordinal))
        {
            return false;
        }

        var branch = value[prefix.Length..];
        if (branch.Length == 0 || branch[0] == '-' || branch.Equals("HEAD", StringComparison.Ordinal) ||
            branch.StartsWith('/') || branch.EndsWith('/') || branch.EndsWith('.') ||
            branch.Contains("..", StringComparison.Ordinal) || branch.Contains("//", StringComparison.Ordinal) ||
            branch.Contains("@{", StringComparison.Ordinal) || branch.Split('/').Any(part => part.EndsWith(".lock", StringComparison.OrdinalIgnoreCase)))
        {
            return false;
        }

        return branch.All(character => character > ' ' && character != '\u007f' &&
            character is not ('~' or '^' or ':' or '?' or '*' or '[' or '\\'));
    }

    private static bool IsValidPolicyPath(string value)
    {
        if (string.IsNullOrWhiteSpace(value) || value.StartsWith('/') || value.EndsWith('/') ||
            value.Contains('\\') || value.Contains(':') || value.Any(character => char.IsControl(character)))
        {
            return false;
        }

        return value.Split('/').All(part => part.Length > 0 && part is not "." and not "..");
    }

    private static bool PathsEqual(string left, string right)
    {
        return string.Equals(
            Path.GetFullPath(left),
            Path.GetFullPath(right),
            OperatingSystem.IsWindows() ? StringComparison.OrdinalIgnoreCase : StringComparison.Ordinal);
    }
}
