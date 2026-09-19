using System.Text.Json;

namespace UnrealBuildTools;

public sealed record ModuleCompatibilityResult(string ExpectedBuildId, IReadOnlyList<string> Problems);

public sealed class EditorModuleCompatibilityChecker
{
    private static readonly JsonSerializerOptions json_options = new()
    {
        PropertyNameCaseInsensitive = true,
    };

    public ModuleCompatibilityResult Check(string engine_root, IEnumerable<string> manifest_paths)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(engine_root);
        ArgumentNullException.ThrowIfNull(manifest_paths);

        var version_path = Path.Combine(engine_root, "Engine", "Binaries", "Win64", "UnrealEditor.version");
        if (!File.Exists(version_path))
        {
            throw new ToolInputException($"The editor version file is missing: '{version_path}'.");
        }

        var expected_build_id = ReadBuildId(version_path, "editor version file");
        if (string.IsNullOrWhiteSpace(expected_build_id))
        {
            throw new ToolInputException($"The editor version file does not contain a BuildId: '{version_path}'.");
        }

        var problems = new List<string>();
        foreach (var manifest_path in manifest_paths)
        {
            if (!File.Exists(manifest_path))
            {
                problems.Add($"missing '{manifest_path}'");
                continue;
            }

            var actual_build_id = ReadBuildId(manifest_path, "module manifest");
            if (string.IsNullOrWhiteSpace(actual_build_id))
            {
                problems.Add($"'{manifest_path}' does not contain a BuildId");
                continue;
            }

            if (!string.Equals(actual_build_id, expected_build_id, StringComparison.Ordinal))
            {
                problems.Add($"'{manifest_path}' has BuildId {actual_build_id}; editor expects {expected_build_id}");
            }
        }

        return new ModuleCompatibilityResult(expected_build_id, problems);
    }

    private static string? ReadBuildId(string path, string description)
    {
        try
        {
            var version = JsonSerializer.Deserialize<BuildVersion>(File.ReadAllText(path), json_options);
            if (version is null)
            {
                throw new ToolInputException($"The {description} JSON is null: '{path}'.");
            }

            return version.BuildId;
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

    private sealed class BuildVersion
    {
        public string? BuildId { get; init; }
    }
}
