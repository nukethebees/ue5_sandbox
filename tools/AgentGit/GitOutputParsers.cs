using System.Text;

namespace AgentGit;

internal static class GitOutputParsers
{
    private static readonly UTF8Encoding strict_utf8 = new(
        encoderShouldEmitUTF8Identifier: false,
        throwOnInvalidBytes: true);

    public static WorkingTreeStatus ParseStatus(byte[] output)
    {
        var records = ParseNullDelimited(output, "Git status");
        var staged = false;
        var unstaged = false;
        var untracked = false;
        var conflicts = false;

        for (var index = 0; index < records.Count; ++index)
        {
            var record = records[index];
            switch (record[0])
            {
                case '1':
                    ParseOrdinaryRecord(record, ref staged, ref unstaged);
                    break;

                case '2':
                    ParseRenameRecord(record, ref staged, ref unstaged);
                    ++index;
                    if (index >= records.Count || records[index].Length == 0)
                    {
                        throw new RepositoryException("Git status returned a truncated rename/copy record.");
                    }
                    break;

                case 'u':
                    ParseUnmergedRecord(record);
                    conflicts = true;
                    staged = true;
                    unstaged = true;
                    break;

                case '?':
                    ParsePathRecord(record, '?');
                    untracked = true;
                    break;

                default:
                    throw new RepositoryException(
                        $"Git status returned an unsupported porcelain-v2 record '{record}'.");
            }
        }

        return new WorkingTreeStatus(staged, unstaged, untracked, conflicts);
    }

    public static IReadOnlyList<string> ParseConfigNames(byte[] output)
    {
        var names = ParseNullDelimited(output, "Git config");
        foreach (var name in names)
        {
            var separator = name.IndexOf('.');
            if (separator <= 0 || separator == name.Length - 1 ||
                char.IsWhiteSpace(name[0]) || char.IsWhiteSpace(name[^1]) ||
                name.Any(character => char.IsControl(character)))
            {
                throw new RepositoryStateException(
                    $"Git config returned a malformed configuration name '{name}'.");
            }
        }

        return names;
    }

    private static IReadOnlyList<string> ParseNullDelimited(byte[] output, string source)
    {
        if (output.Length == 0)
        {
            return [];
        }

        string text;
        try
        {
            text = strict_utf8.GetString(output);
        }
        catch (DecoderFallbackException exception)
        {
            throw new RepositoryStateException($"{source} returned invalid UTF-8 output.", exception);
        }

        if (text[^1] != '\0')
        {
            throw new RepositoryStateException($"{source} returned unterminated NUL-delimited output.");
        }

        var records = text.Split('\0');
        if (records[^1].Length != 0 || records[..^1].Any(record => record.Length == 0))
        {
            throw new RepositoryStateException($"{source} returned an empty NUL-delimited record.");
        }

        return records[..^1];
    }

    private static void ParseOrdinaryRecord(string record, ref bool staged, ref bool unstaged)
    {
        var fields = record.Split(' ', 9);
        if (fields.Length != 9 || fields[0] != "1" || !IsOrdinaryChangeCode(fields[1]) ||
            !IsSubmoduleState(fields[2]) || !IsMode(fields[3]) || !IsMode(fields[4]) ||
            !IsMode(fields[5]) || !IsObjectId(fields[6]) || !IsObjectId(fields[7]) ||
            fields[8].Length == 0)
        {
            throw new RepositoryException($"Git status returned malformed ordinary record '{record}'.");
        }

        staged |= fields[1][0] != '.';
        unstaged |= fields[1][1] != '.';
    }

    private static void ParseRenameRecord(string record, ref bool staged, ref bool unstaged)
    {
        var fields = record.Split(' ', 10);
        if (fields.Length != 10 || fields[0] != "2" || !IsRenameChangeCode(fields[1]) ||
            !IsSubmoduleState(fields[2]) || !IsMode(fields[3]) || !IsMode(fields[4]) ||
            !IsMode(fields[5]) || !IsObjectId(fields[6]) || !IsObjectId(fields[7]) ||
            !IsRenameScore(fields[8]) || fields[9].Length == 0)
        {
            throw new RepositoryException($"Git status returned malformed rename/copy record '{record}'.");
        }

        staged |= fields[1][0] != '.';
        unstaged |= fields[1][1] != '.';
    }

    private static void ParseUnmergedRecord(string record)
    {
        var fields = record.Split(' ', 11);
        if (fields.Length != 11 || fields[0] != "u" || !IsUnmergedChangeCode(fields[1]) ||
            !IsSubmoduleState(fields[2]) || !IsMode(fields[3]) || !IsMode(fields[4]) ||
            !IsMode(fields[5]) || !IsMode(fields[6]) || !IsObjectId(fields[7]) ||
            !IsObjectId(fields[8]) || !IsObjectId(fields[9]) || fields[10].Length == 0)
        {
            throw new RepositoryException($"Git status returned malformed unmerged record '{record}'.");
        }
    }

    private static void ParsePathRecord(string record, char prefix)
    {
        if (record.Length < 3 || record[0] != prefix || record[1] != ' ')
        {
            throw new RepositoryException($"Git status returned malformed path record '{record}'.");
        }
    }

    private static bool IsOrdinaryChangeCode(string value)
    {
        const string status_characters = ".MTADU";
        return value.Length == 2 && value.All(status_characters.Contains);
    }

    private static bool IsRenameChangeCode(string value)
    {
        const string status_characters = ".MTADRCU";
        return value.Length == 2 && value.All(status_characters.Contains) &&
            (value[0] is 'R' or 'C' || value[1] is 'R' or 'C');
    }

    private static bool IsUnmergedChangeCode(string value)
    {
        return value is "DD" or "AU" or "UD" or "UA" or "DU" or "AA" or "UU";
    }

    private static bool IsSubmoduleState(string value)
    {
        return value == "N..." ||
            value.Length == 4 && value[0] == 'S' &&
            (value[1] is '.' or 'C') &&
            (value[2] is '.' or 'M') &&
            (value[3] is '.' or 'U');
    }

    private static bool IsMode(string value)
    {
        return value.Length == 6 && value.All(character => character is >= '0' and <= '7');
    }

    private static bool IsObjectId(string value)
    {
        return value.Length is 40 or 64 && value.All(char.IsAsciiHexDigit);
    }

    private static bool IsRenameScore(string value)
    {
        return value.Length is >= 2 and <= 4 && (value[0] is 'R' or 'C') &&
            int.TryParse(value[1..], out var score) && score is >= 0 and <= 100;
    }
}
