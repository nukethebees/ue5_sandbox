namespace ArchitectureChecks;

internal sealed class CoreRedirectParser
{
    public IReadOnlySet<CoreRedirect> Parse(string text)
    {
        ArgumentNullException.ThrowIfNull(text);

        var redirects = new HashSet<CoreRedirect>();
        using var reader = new StringReader(text);
        while (reader.ReadLine() is { } line)
        {
            TryAddRedirect(line, redirects);
        }

        return redirects;
    }

    private static void TryAddRedirect(string line, ISet<CoreRedirect> redirects)
    {
        var trimmed = line.TrimStart();
        foreach (var kind in Enum.GetValues<ReflectedTypeKind>())
        {
            var prefix = $"+{kind}Redirects=";
            if (!trimmed.StartsWith(prefix, StringComparison.Ordinal))
            {
                continue;
            }

            var fields = ParseFields(trimmed[prefix.Length..]);
            if (fields.TryGetValue("OldName", out var old_name) && fields.TryGetValue("NewName", out var new_name))
            {
                redirects.Add(new CoreRedirect(kind, old_name, new_name));
            }

            return;
        }
    }

    private static IReadOnlyDictionary<string, string> ParseFields(string value)
    {
        var fields = new Dictionary<string, string>(StringComparer.Ordinal);
        var index = 0;
        SkipWhitespace(value, ref index);
        if (index >= value.Length || value[index++] != '(')
        {
            return fields;
        }

        while (index < value.Length)
        {
            SkipWhitespace(value, ref index);
            if (index < value.Length && value[index] == ')')
            {
                return fields;
            }

            var key_start = index;
            while (index < value.Length && (char.IsAsciiLetterOrDigit(value[index]) || value[index] == '_'))
            {
                ++index;
            }

            if (key_start == index)
            {
                return fields;
            }

            var key = value[key_start..index];
            SkipWhitespace(value, ref index);
            if (index >= value.Length || value[index++] != '=')
            {
                return fields;
            }

            SkipWhitespace(value, ref index);
            if (index >= value.Length || value[index++] != '"')
            {
                return fields;
            }

            var field_value = ReadQuotedValue(value, ref index);
            if (field_value is null)
            {
                return fields;
            }

            fields[key] = field_value;
            SkipWhitespace(value, ref index);
            if (index < value.Length && value[index] == ',')
            {
                ++index;
            }
        }

        return fields;
    }

    private static string? ReadQuotedValue(string value, ref int index)
    {
        var characters = new List<char>();
        while (index < value.Length)
        {
            var character = value[index++];
            if (character == '"')
            {
                return new string(characters.ToArray());
            }

            if (character == '\\' && index < value.Length)
            {
                character = value[index++];
            }

            characters.Add(character);
        }

        return null;
    }

    private static void SkipWhitespace(string value, ref int index)
    {
        while (index < value.Length && char.IsWhiteSpace(value[index]))
        {
            ++index;
        }
    }
}

internal sealed record CoreRedirect(ReflectedTypeKind Kind, string OldName, string NewName);
