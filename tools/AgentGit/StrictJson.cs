using System.Text.Json;

namespace AgentGit;

internal static class StrictJson
{
    public static void RejectDuplicateProperties(string json)
    {
        using var document = JsonDocument.Parse(json);
        ValidateElement(document.RootElement, "$");
    }

    private static void ValidateElement(JsonElement element, string path)
    {
        if (element.ValueKind == JsonValueKind.Object)
        {
            var names = new HashSet<string>(StringComparer.Ordinal);
            foreach (var property in element.EnumerateObject())
            {
                if (!names.Add(property.Name))
                {
                    throw new JsonException($"Duplicate JSON property '{property.Name}' at '{path}'.");
                }

                ValidateElement(property.Value, $"{path}.{property.Name}");
            }
        }
        else if (element.ValueKind == JsonValueKind.Array)
        {
            var index = 0;
            foreach (var item in element.EnumerateArray())
            {
                ValidateElement(item, $"{path}[{index}]");
                ++index;
            }
        }
    }
}
