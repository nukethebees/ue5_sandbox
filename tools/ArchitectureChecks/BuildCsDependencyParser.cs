using System.Text.RegularExpressions;

namespace ArchitectureChecks;

public sealed class BuildCsDependencyParser
{
    private static readonly Regex comments = new(
        @"//[^\n]*|/\*.*?\*/",
        RegexOptions.Compiled | RegexOptions.CultureInvariant | RegexOptions.Singleline);

    private static readonly Regex dependency_calls = new(
        @"(?:Public|Private)DependencyModuleNames\.Add(?:Range)?\s*\((?<arguments>.*?)\);",
        RegexOptions.Compiled | RegexOptions.CultureInvariant | RegexOptions.Singleline);

    private static readonly Regex module_names = new(
        "\\\"(?<name>\\w+)\\\"",
        RegexOptions.Compiled | RegexOptions.CultureInvariant);

    public IReadOnlySet<string> Parse(string text)
    {
        ArgumentNullException.ThrowIfNull(text);

        var uncommented = comments.Replace(text, string.Empty);
        var dependencies = new HashSet<string>(StringComparer.Ordinal);
        foreach (Match dependency_call in dependency_calls.Matches(uncommented))
        {
            var arguments = dependency_call.Groups["arguments"].Value;
            foreach (Match module_name in module_names.Matches(arguments))
            {
                dependencies.Add(module_name.Groups["name"].Value);
            }
        }

        return dependencies;
    }
}
