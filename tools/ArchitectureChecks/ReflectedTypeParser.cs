namespace ArchitectureChecks;

internal static class ReflectedTypeParser
{
    public static IReadOnlyList<ReflectedType> Parse(string text)
    {
        ArgumentNullException.ThrowIfNull(text);

        var tokens = CppTokenReader.Read(text);
        var types = new List<ReflectedType>();
        for (var index = 0; index < tokens.Count; ++index)
        {
            var kind = tokens[index] switch
            {
                "UCLASS" or "UINTERFACE" => ReflectedTypeKind.Class,
                "USTRUCT" => ReflectedTypeKind.Struct,
                "UENUM" => ReflectedTypeKind.Enum,
                _ => (ReflectedTypeKind?)null,
            };
            if (kind is null)
            {
                continue;
            }

            var declaration_index = index + 1;
            if (declaration_index < tokens.Count && tokens[declaration_index] == "(")
            {
                declaration_index = SkipBalancedParentheses(tokens, declaration_index);
            }

            if (declaration_index >= tokens.Count)
            {
                continue;
            }

            if (kind == ReflectedTypeKind.Class && tokens[declaration_index] != "class" ||
                kind == ReflectedTypeKind.Struct && tokens[declaration_index] != "struct" ||
                kind == ReflectedTypeKind.Enum && tokens[declaration_index] != "enum")
            {
                continue;
            }

            ++declaration_index;
            if (kind == ReflectedTypeKind.Enum && declaration_index < tokens.Count && tokens[declaration_index] == "class")
            {
                ++declaration_index;
            }

            if (declaration_index < tokens.Count && tokens[declaration_index].EndsWith("_API", StringComparison.Ordinal))
            {
                ++declaration_index;
            }

            if (declaration_index >= tokens.Count || !IsExpectedName(kind.Value, tokens[declaration_index]))
            {
                continue;
            }

            types.Add(new ReflectedType(kind.Value, tokens[declaration_index]));
        }

        return types;
    }

    private static int SkipBalancedParentheses(IReadOnlyList<string> tokens, int opening_index)
    {
        var depth = 0;
        for (var index = opening_index; index < tokens.Count; ++index)
        {
            if (tokens[index] == "(")
            {
                ++depth;
            }
            else if (tokens[index] == ")" && --depth == 0)
            {
                return index + 1;
            }
        }

        return tokens.Count;
    }

    private static bool IsExpectedName(ReflectedTypeKind kind, string name)
    {
        if (string.IsNullOrEmpty(name))
        {
            return false;
        }

        return kind switch
        {
            ReflectedTypeKind.Class => name.StartsWith('A') || name.StartsWith('U'),
            ReflectedTypeKind.Struct => name.StartsWith('F'),
            ReflectedTypeKind.Enum => name.StartsWith('E'),
            _ => false,
        };
    }

    private static class CppTokenReader
    {
        public static IReadOnlyList<string> Read(string text)
        {
            var tokens = new List<string>();
            for (var index = 0; index < text.Length;)
            {
                if (char.IsWhiteSpace(text[index]))
                {
                    ++index;
                }
                else if (text[index] == '/' && index + 1 < text.Length && text[index + 1] == '/')
                {
                    index = SkipLineComment(text, index + 2);
                }
                else if (text[index] == '/' && index + 1 < text.Length && text[index + 1] == '*')
                {
                    index = SkipBlockComment(text, index + 2);
                }
                else if (text[index] == '"' || text[index] == '\'')
                {
                    index = SkipQuotedLiteral(text, index + 1, text[index]);
                }
                else if (char.IsAsciiLetter(text[index]) || text[index] == '_')
                {
                    var start = index++;
                    while (index < text.Length && (char.IsAsciiLetterOrDigit(text[index]) || text[index] == '_'))
                    {
                        ++index;
                    }

                    tokens.Add(text[start..index]);
                }
                else
                {
                    tokens.Add(text[index++].ToString());
                }
            }

            return tokens;
        }

        private static int SkipLineComment(string text, int index)
        {
            while (index < text.Length && text[index] != '\n')
            {
                ++index;
            }

            return index;
        }

        private static int SkipBlockComment(string text, int index)
        {
            while (index + 1 < text.Length && (text[index] != '*' || text[index + 1] != '/'))
            {
                ++index;
            }

            return Math.Min(index + 2, text.Length);
        }

        private static int SkipQuotedLiteral(string text, int index, char quote)
        {
            while (index < text.Length)
            {
                if (text[index] == '\\')
                {
                    index += 2;
                }
                else if (text[index++] == quote)
                {
                    break;
                }
            }

            return Math.Min(index, text.Length);
        }
    }
}
