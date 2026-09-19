namespace CodeFormatTools;

internal enum FormatMode
{
    All,
    Changed,
    Staged,
}

internal sealed record FormatRequest(FormatMode Mode, bool Verbose)
{
    public static bool TryParse(
        IReadOnlyList<string> arguments,
        out FormatRequest? request,
        out string? error)
    {
        ArgumentNullException.ThrowIfNull(arguments);

        FormatMode? mode = null;
        var verbose = false;

        foreach (var argument in arguments)
        {
            if (string.Equals(argument, "--verbose", StringComparison.Ordinal))
            {
                verbose = true;
                continue;
            }

            FormatMode? requested_mode = argument switch
            {
                "--all" => FormatMode.All,
                "--changed" => FormatMode.Changed,
                "--staged" => FormatMode.Staged,
                _ => null,
            };
            if (requested_mode is null)
            {
                request = null;
                error = $"Unknown argument '{argument}'.";
                return false;
            }

            if (mode is not null && mode != requested_mode)
            {
                request = null;
                error = "Only one of --all, --changed, or --staged may be specified.";
                return false;
            }

            mode = requested_mode;
        }

        request = new FormatRequest(mode ?? FormatMode.All, verbose);
        error = null;
        return true;
    }
}
