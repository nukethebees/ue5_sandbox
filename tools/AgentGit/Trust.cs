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
        TrustManifest manifest;
        try
        {
            manifest = JsonSerializer.Deserialize<TrustManifest>(File.ReadAllText(manifest_path), json_options)
                ?? throw new PolicyConfigurationException("Trust manifest contained null.");
        }
        catch (Exception exception) when (exception is IOException or UnauthorizedAccessException or JsonException)
        {
            throw new PolicyConfigurationException(
                $"Unable to load agent-git trust manifest '{manifest_path}': {exception.Message}", exception);
        }

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

        if (string.IsNullOrWhiteSpace(manifest.UserName) || string.IsNullOrWhiteSpace(manifest.UserEmail))
        {
            throw new PolicyConfigurationException("Trust manifest must contain non-empty Git user identity.");
        }

        if (manifest.Repositories.Length == 0)
        {
            throw new PolicyConfigurationException("Trust manifest does not register any repositories.");
        }

        var empty_config = Path.Combine(installation_root, "config", "empty.gitconfig");
        var empty_hooks = Path.Combine(installation_root, "config", "empty-hooks");
        var empty_attributes = Path.Combine(installation_root, "config", "empty.attributes");
        if (!File.Exists(empty_config) || !File.Exists(empty_attributes) ||
            !Directory.Exists(empty_hooks) || Directory.EnumerateFileSystemEntries(empty_hooks).Any())
        {
            throw new PolicyConfigurationException("The canonical empty Git configuration or hook directory is missing or invalid.");
        }

        return new TrustContext(
            installation_root,
            Path.GetFullPath(manifest.GitExecutable),
            manifest.GitLfsExecutable is null ? null : Path.GetFullPath(manifest.GitLfsExecutable),
            manifest.UserName,
            manifest.UserEmail,
            empty_config,
            empty_hooks,
            manifest.Repositories);
    }

    private static void ValidateFile(string path, string description)
    {
        if (string.IsNullOrWhiteSpace(path) || !Path.IsPathFullyQualified(path) || !File.Exists(path))
        {
            throw new PolicyConfigurationException($"{description} is not an existing absolute file: '{path}'.");
        }
    }

    private static bool PathsEqual(string left, string right)
    {
        return string.Equals(
            Path.GetFullPath(left),
            Path.GetFullPath(right),
            OperatingSystem.IsWindows() ? StringComparison.OrdinalIgnoreCase : StringComparison.Ordinal);
    }
}
