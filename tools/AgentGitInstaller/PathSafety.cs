namespace AgentGitInstaller;

internal static class PathSafety
{
    private static readonly StringComparison path_comparison = OperatingSystem.IsWindows()
        ? StringComparison.OrdinalIgnoreCase
        : StringComparison.Ordinal;

    public static string ExistingDirectory(string value, string description, IInstallerFileSystem file_system)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(value);

        var path = Path.GetFullPath(value);
        if (!file_system.DirectoryExists(path))
        {
            var actual = file_system.FileExists(path) ? "it is a file" : "it does not exist";
            throw new InstallerException($"{description} must be an existing directory, but {actual}: '{path}'.");
        }

        return Path.TrimEndingDirectorySeparator(path);
    }

    public static void EnsureSafeInstallRoot(string path)
    {
        var root = Path.GetPathRoot(path);
        if (string.IsNullOrWhiteSpace(root) || PathsEqual(root, path) ||
            string.IsNullOrWhiteSpace(Path.GetFileName(Path.TrimEndingDirectorySeparator(path))))
        {
            throw new InstallerException($"Installation root must name a child directory, not a filesystem root: '{path}'.");
        }
    }

    public static void EnsureNoReparsePoints(
        string path,
        string description,
        IInstallerFileSystem file_system)
    {
        var current = Path.GetFullPath(path);
        while (!string.IsNullOrWhiteSpace(current))
        {
            var attributes = TryGetAttributes(current, file_system);
            if (attributes.HasValue && (attributes.Value & FileAttributes.ReparsePoint) != 0)
            {
                throw new InstallerException(
                    $"{description} cannot contain a symbolic link or reparse point: '{current}'.");
            }

            var parent = Path.GetDirectoryName(current);
            if (string.IsNullOrWhiteSpace(parent) || PathsEqual(parent, current))
            {
                break;
            }

            current = parent;
        }
    }

    public static bool IsStrictlyWithin(string root, string candidate)
    {
        var relative = Path.GetRelativePath(Path.GetFullPath(root), Path.GetFullPath(candidate));
        return relative != "." && !Path.IsPathFullyQualified(relative) && relative != ".." &&
            !relative.StartsWith($"..{Path.DirectorySeparatorChar}", path_comparison) &&
            !relative.StartsWith($"..{Path.AltDirectorySeparatorChar}", path_comparison);
    }

    public static bool PathsEqual(string left, string right)
    {
        return string.Equals(
            Path.TrimEndingDirectorySeparator(Path.GetFullPath(left)),
            Path.TrimEndingDirectorySeparator(Path.GetFullPath(right)),
            path_comparison);
    }

    public static void ValidatePolicyPath(string value)
    {
        if (string.IsNullOrWhiteSpace(value) || value.StartsWith('/') || value.EndsWith('/') ||
            value.Contains('\\') || value.Contains(':') || value.Any(char.IsControl) ||
            value.Split('/').Any(part => part.Length == 0 || part is "." or ".."))
        {
            throw new InstallerException(
                $"Policy path must be a safe repository-relative Git path; found '{value}'.");
        }
    }

    private static FileAttributes? TryGetAttributes(string path, IInstallerFileSystem file_system)
    {
        try
        {
            return file_system.GetAttributes(path);
        }
        catch (Exception exception) when (exception is FileNotFoundException or DirectoryNotFoundException)
        {
            return null;
        }
    }
}
