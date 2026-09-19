using System.Text.RegularExpressions;

namespace NativeBinaryTools;

internal static partial class MimallocSymbolParser
{
    public static IReadOnlySet<string> AllocatorIdentifiers(string symbolOutput)
    {
        ArgumentNullException.ThrowIfNull(symbolOutput);

        var identifiers = new HashSet<string>(StringComparer.Ordinal);
        foreach (Match match in MimallocIdentifierRegex().Matches(symbolOutput))
        {
            identifiers.Add(match.Groups[1].Value);
        }

        return identifiers;
    }

    public static IReadOnlySet<string> LinkerIncludes(string directiveOutput)
    {
        ArgumentNullException.ThrowIfNull(directiveOutput);

        var includes = new HashSet<string>(StringComparer.Ordinal);
        foreach (Match match in IncludeDirectiveRegex().Matches(directiveOutput))
        {
            includes.Add(match.Groups[1].Value);
        }

        return includes;
    }

    public static string WithoutMsvcStringLiterals(string symbolOutput)
    {
        ArgumentNullException.ThrowIfNull(symbolOutput);

        return string.Join(
            "\n",
            symbolOutput.Split(["\r\n", "\n", "\r"], StringSplitOptions.None)
                .Where(line => !MsvcStringLiteralRegex().IsMatch(line)));
    }

    public static bool IsMimallocLinkerInclude(string symbol)
    {
        ArgumentNullException.ThrowIfNull(symbol);

        return MimallocIncludeRegex().IsMatch(symbol);
    }

    public static bool IsMsvcOperatorNewDelete(string symbol)
    {
        ArgumentNullException.ThrowIfNull(symbol);

        return MsvcOperatorNewDeleteRegex().IsMatch(symbol);
    }

    public static string Prefixed(string identifier)
    {
        ArgumentNullException.ThrowIfNull(identifier);

        return $"sbx_{identifier}";
    }

    [GeneratedRegex(@"(?<![A-Za-z0-9_])(_*mi_[A-Za-z0-9_]+)", RegexOptions.CultureInvariant)]
    private static partial Regex MimallocIdentifierRegex();

    [GeneratedRegex(@"/INCLUDE:([^\s]+)", RegexOptions.IgnoreCase | RegexOptions.CultureInvariant)]
    private static partial Regex IncludeDirectiveRegex();

    [GeneratedRegex(@"\s\?\?_C@", RegexOptions.CultureInvariant)]
    private static partial Regex MsvcStringLiteralRegex();

    [GeneratedRegex(@"^\?\?(?:[23]|_[UV])@", RegexOptions.CultureInvariant)]
    private static partial Regex MsvcOperatorNewDeleteRegex();

    [GeneratedRegex(@"^_+mi_[A-Za-z0-9_]+$", RegexOptions.CultureInvariant)]
    private static partial Regex MimallocIncludeRegex();
}
