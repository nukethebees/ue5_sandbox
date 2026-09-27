namespace CodeFormatTools;

internal sealed class FormattingScope
{
    private readonly HashSet<string> supported_extensions;
    private readonly HashSet<string> excluded_components;

    private readonly StringComparer path_comparer = OperatingSystem.IsWindows()
        ? StringComparer.OrdinalIgnoreCase
        : StringComparer.Ordinal;

    private readonly StringComparison path_comparison = OperatingSystem.IsWindows()
        ? StringComparison.OrdinalIgnoreCase
        : StringComparison.Ordinal;

    public FormattingScope(string repository_root, FormattingPolicy policy)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(repository_root);
        ArgumentNullException.ThrowIfNull(policy);
        supported_extensions = new(policy.Extensions, StringComparer.OrdinalIgnoreCase);
        excluded_components = new(policy.ExcludedComponents, StringComparer.OrdinalIgnoreCase);

        RepositoryRoot = Path.GetFullPath(repository_root);
        Roots = policy.Roots
            .Select(relative_root => Path.GetFullPath(Path.Combine(RepositoryRoot, relative_root)))
            .ToArray();
    }

    public string RepositoryRoot { get; }

    public IReadOnlyList<string> Roots { get; }

    public bool IsFormatCandidate(string file_path)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(file_path);

        var full_path = Path.GetFullPath(file_path);
        return File.Exists(full_path) &&
               supported_extensions.Contains(Path.GetExtension(full_path)) &&
               !IsExcludedPath(full_path) &&
               Roots.Any(root => IsWithinDirectory(full_path, root));
    }

    public IReadOnlyList<string> SelectAll(Action<string> warning)
    {
        ArgumentNullException.ThrowIfNull(warning);

        var files = new HashSet<string>(path_comparer);
        foreach (var root in Roots)
        {
            if (!Directory.Exists(root))
            {
                warning($"WARNING: Directory not found: {root}");
                continue;
            }

            try
            {
                foreach (var file_path in Directory.EnumerateFiles(root, "*", SearchOption.AllDirectories))
                {
                    if (IsFormatCandidate(file_path))
                    {
                        files.Add(Path.GetFullPath(file_path));
                    }
                }
            }
            catch (Exception exception) when (exception is UnauthorizedAccessException or IOException)
            {
                warning($"WARNING: Failed to scan {root}: {exception.Message}");
            }
        }

        return files.OrderBy(file_path => file_path, path_comparer).ToArray();
    }

    private bool IsExcludedPath(string file_path)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(file_path);

        return Path.GetRelativePath(RepositoryRoot, file_path)
            .Split([Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar], StringSplitOptions.RemoveEmptyEntries)
            .Any(component => excluded_components.Contains(component));
    }

    private bool IsWithinDirectory(string file_path, string directory)
    {
        var relative_path = Path.GetRelativePath(directory, file_path);
        return !string.Equals(relative_path, "..", path_comparison) &&
               !relative_path.StartsWith($"..{Path.DirectorySeparatorChar}", path_comparison) &&
               !relative_path.StartsWith($"..{Path.AltDirectorySeparatorChar}", path_comparison) &&
               !Path.IsPathRooted(relative_path);
    }
}
