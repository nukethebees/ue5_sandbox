using System.Text.Json;

namespace UnrealBuildTools;

public sealed record ManifestDiscoveryResult(IReadOnlyList<string> Paths, string? Problem);

public sealed class EditorModuleManifestLocator
{
    private static readonly JsonSerializerOptions json_options = new()
    {
        PropertyNameCaseInsensitive = true,
    };

    public static string GetManifestName(string configuration)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(configuration);
        return string.Equals(configuration, "Development", StringComparison.OrdinalIgnoreCase)
            ? "UnrealEditor.modules"
            : $"UnrealEditor-Win64-{configuration}.modules";
    }

    public static string GetTargetReceiptName(string target, string configuration)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(target);
        ArgumentException.ThrowIfNullOrWhiteSpace(configuration);
        return string.Equals(configuration, "Development", StringComparison.OrdinalIgnoreCase)
            ? $"{target}.target"
            : $"{target}-Win64-{configuration}.target";
    }

    public ManifestDiscoveryResult Discover(string project_root, string target, string configuration)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(project_root);

        var normalized_project_root = Path.GetFullPath(project_root);
        var manifest_name = GetManifestName(configuration);
        var receipt_path = Path.Combine(
            normalized_project_root,
            "Binaries",
            "Win64",
            GetTargetReceiptName(target, configuration));
        if (!File.Exists(receipt_path))
        {
            return new ManifestDiscoveryResult([], $"The editor target receipt is missing: {receipt_path}");
        }

        var receipt = ReadJson<TargetReceipt>(receipt_path, "editor target receipt");
        if (receipt.BuildProducts is null)
        {
            throw new ToolInputException($"The editor target receipt does not contain BuildProducts: '{receipt_path}'.");
        }

        var paths = new List<string>();
        var unique_paths = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        AddManifestPath(Path.Combine(normalized_project_root, "Binaries", "Win64", manifest_name));

        var plugin_root = Path.Combine(normalized_project_root, "Plugins");
        foreach (var build_product in receipt.BuildProducts)
        {
            var product_path = build_product?.Path;
            if (string.IsNullOrWhiteSpace(product_path))
            {
                continue;
            }

            var expanded_path = product_path
                .Replace("$(ProjectDir)", normalized_project_root, StringComparison.Ordinal)
                .Replace('/', Path.DirectorySeparatorChar);
            var normalized_product_path = Path.GetFullPath(expanded_path);
            if (!IsWithinDirectory(normalized_product_path, plugin_root))
            {
                continue;
            }

            var binary_directory = Path.GetDirectoryName(normalized_product_path);
            if (string.IsNullOrWhiteSpace(binary_directory))
            {
                throw new ToolInputException($"Unable to determine the directory for receipt build product '{product_path}'.");
            }

            AddManifestPath(Path.Combine(binary_directory, manifest_name));
        }

        return new ManifestDiscoveryResult(paths, null);

        void AddManifestPath(string path)
        {
            var normalized_path = Path.GetFullPath(path);
            if (unique_paths.Add(normalized_path))
            {
                paths.Add(normalized_path);
            }
        }
    }

    private static bool IsWithinDirectory(string path, string directory)
    {
        var normalized_path = Path.TrimEndingDirectorySeparator(Path.GetFullPath(path));
        var normalized_directory = Path.TrimEndingDirectorySeparator(Path.GetFullPath(directory));
        if (string.Equals(normalized_path, normalized_directory, StringComparison.OrdinalIgnoreCase))
        {
            return true;
        }

        return normalized_path.StartsWith(
            string.Concat(normalized_directory, Path.DirectorySeparatorChar),
            StringComparison.OrdinalIgnoreCase);
    }

    private static T ReadJson<T>(string path, string description)
        where T : class
    {
        try
        {
            var value = JsonSerializer.Deserialize<T>(File.ReadAllText(path), json_options);
            return value ?? throw new ToolInputException($"The {description} JSON is null: '{path}'.");
        }
        catch (JsonException exception)
        {
            throw new ToolInputException($"The {description} contains invalid JSON: '{path}'.", exception);
        }
        catch (IOException exception)
        {
            throw new ToolInputException($"Unable to read the {description}: '{path}'.", exception);
        }
    }

    private sealed class TargetReceipt
    {
        public List<BuildProduct?>? BuildProducts { get; init; }
    }

    private sealed class BuildProduct
    {
        public string? Path { get; init; }
    }
}
