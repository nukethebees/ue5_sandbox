using System.Text;
using System.Text.RegularExpressions;

namespace SetLiveCodingDisabled;

public sealed class LiveCodingSettingsNormalizer
{
    private const string target_section = "/Script/LiveCoding.LiveCodingSettings";

    private static readonly Regex line_break_regex = new(@"(\r\n|\n|\r)");
    private static readonly Regex section_regex = new(@"^\s*\[([^\]]+)\]\s*$");
    private static readonly Regex enabled_regex = new(
        @"^(\s*bEnabled\s*=\s*)[^;#\s]+(.*)$",
        RegexOptions.IgnoreCase);

    public bool Disable(string settings_path)
    {
        ArgumentNullException.ThrowIfNull(settings_path);

        if (!File.Exists(settings_path))
        {
            return false;
        }

        string contents;
        Encoding encoding;
        using (var reader = new StreamReader(settings_path, new UTF8Encoding(false, true), true))
        {
            contents = reader.ReadToEnd();
            encoding = reader.CurrentEncoding;
        }

        var parts = line_break_regex.Split(contents);
        var in_live_coding_section = false;
        var changed = false;

        for (var index = 0; index < parts.Length; index += 2)
        {
            var line = parts[index];
            var section_match = section_regex.Match(line);
            if (section_match.Success)
            {
                in_live_coding_section = string.Equals(
                    section_match.Groups[1].Value,
                    target_section,
                    StringComparison.OrdinalIgnoreCase);
                continue;
            }

            if (!in_live_coding_section)
            {
                continue;
            }

            var enabled_match = enabled_regex.Match(line);
            if (!enabled_match.Success)
            {
                continue;
            }

            var updated_line = string.Concat(
                enabled_match.Groups[1].Value,
                "False",
                enabled_match.Groups[2].Value);
            if (string.Equals(updated_line, line, StringComparison.Ordinal))
            {
                continue;
            }

            parts[index] = updated_line;
            changed = true;
        }

        if (!changed)
        {
            return false;
        }

        File.WriteAllText(settings_path, string.Concat(parts), encoding);
        return true;
    }
}
