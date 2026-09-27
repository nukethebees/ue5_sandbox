using System.Text.Json;

namespace CodeFormatTools;

internal sealed record FormattingPolicy
{
    public required string[] Roots { get; init; }
    public required string[] Extensions { get; init; }
    public required string[] ExcludedComponents { get; init; }

    public static FormattingPolicy Load(string repository_root)
    {
        var path = Path.Combine(repository_root, ".code-format.json");
        try
        {
            var policy = JsonSerializer.Deserialize<FormattingPolicy>(File.ReadAllText(path));
            if (policy is null || policy.Roots is null || policy.Extensions is null || policy.ExcludedComponents is null ||
                policy.Roots.Any(string.IsNullOrWhiteSpace) || policy.Extensions.Any(string.IsNullOrWhiteSpace) ||
                policy.ExcludedComponents.Any(string.IsNullOrWhiteSpace))
            {
                throw new FormatToolException($"Invalid formatting policy: {path}");
            }

            foreach (var root in policy.Roots)
            {
                var relative = Path.GetRelativePath(repository_root, Path.GetFullPath(Path.Combine(repository_root, root)));
                if (Path.IsPathRooted(root) || relative == ".." || relative.StartsWith($"..{Path.DirectorySeparatorChar}"))
                {
                    throw new FormatToolException($"Formatting root must stay inside the worktree: {root}");
                }
            }

            return policy;
        }
        catch (Exception exception) when (exception is JsonException or IOException or UnauthorizedAccessException)
        {
            throw new FormatToolException($"Cannot read formatting policy '{path}': {exception.Message}", exception);
        }
    }
}
