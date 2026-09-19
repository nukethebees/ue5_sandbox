namespace GamePackageTools;

public sealed class PackagePathConverter
{
    private readonly string projectContentDirectory;

    public PackagePathConverter(string projectRoot)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(projectRoot);

        projectContentDirectory = NormalizeDirectory(Path.Combine(projectRoot, "Content"));
    }

    public string ToUnrealPackagePath(string filePath)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(filePath);

        var fullFilePath = Path.GetFullPath(filePath);
        var contentDirectory = FindContentDirectory(Path.GetDirectoryName(fullFilePath));
        if (contentDirectory is null)
        {
            throw new PackageVerificationException($"Asset is not below a Content directory: {fullFilePath}");
        }

        var relativePath = Path.GetRelativePath(contentDirectory, fullFilePath);
        var extension = Path.GetExtension(relativePath);
        if (string.IsNullOrEmpty(extension))
        {
            throw new PackageVerificationException($"Asset has no extension: {fullFilePath}");
        }

        var assetPath = relativePath[..^extension.Length].Replace('\\', '/');
        if (PathsEqual(contentDirectory, projectContentDirectory))
        {
            return $"/Game/{assetPath}";
        }

        var pluginDirectory = Directory.GetParent(contentDirectory)?.FullName;
        if (pluginDirectory is null)
        {
            throw new PackageVerificationException($"Could not find the plugin descriptor for '{fullFilePath}'.");
        }

        var pluginDescriptor = Directory.EnumerateFiles(pluginDirectory, "*.uplugin", SearchOption.TopDirectoryOnly)
            .FirstOrDefault();
        if (pluginDescriptor is null)
        {
            throw new PackageVerificationException($"Could not find the plugin descriptor for '{fullFilePath}'.");
        }

        return $"/{Path.GetFileNameWithoutExtension(pluginDescriptor)}/{assetPath}";
    }

    public static string ToStagedAssetPath(string packagePath)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(packagePath);

        if (!packagePath.StartsWith("/", StringComparison.Ordinal))
        {
            throw new PackageVerificationException($"Invalid Unreal package path: {packagePath}");
        }

        var parts = packagePath[1..].Split('/', 2, StringSplitOptions.None);
        if (parts.Length != 2 ||
            string.IsNullOrWhiteSpace(parts[0]) ||
            string.IsNullOrWhiteSpace(parts[1]) ||
            parts[1].StartsWith("/", StringComparison.Ordinal))
        {
            throw new PackageVerificationException($"Invalid Unreal package path: {packagePath}");
        }

        return string.Equals(parts[0], "Game", StringComparison.Ordinal)
            ? $"../../../Sandbox/Content/{parts[1]}"
            : $"../../../Sandbox/Plugins/{parts[0]}/Content/{parts[1]}";
    }

    private static string? FindContentDirectory(string? directory)
    {
        for (var current = directory; current is not null; current = Directory.GetParent(current)?.FullName)
        {
            if (string.Equals(Path.GetFileName(current), "Content", StringComparison.OrdinalIgnoreCase))
            {
                return NormalizeDirectory(current);
            }
        }

        return null;
    }

    private static bool PathsEqual(string left, string right)
    {
        return string.Equals(NormalizeDirectory(left), NormalizeDirectory(right), StringComparison.OrdinalIgnoreCase);
    }

    private static string NormalizeDirectory(string directory)
    {
        return Path.TrimEndingDirectorySeparator(Path.GetFullPath(directory));
    }
}
