using System.Globalization;

namespace CodeFormatTools;

internal enum FormatMode
{
    All,
    Changed,
    Staged,
}

internal sealed record FormatRequest(FormatMode Mode, bool Verbose, int Jobs)
{
    public static readonly int DefaultJobs = Math.Min(16, Math.Max(1, Environment.ProcessorCount / 2));

    public static bool TryParse(
        IReadOnlyList<string> arguments,
        out FormatRequest? request,
        out string? error)
    {
        ArgumentNullException.ThrowIfNull(arguments);

        FormatMode? mode = null;
        var verbose = false;
        int? jobs = null;

        for (var index = 0; index < arguments.Count; index++)
        {
            var argument = arguments[index];
            if (string.Equals(argument, "--verbose", StringComparison.Ordinal))
            {
                verbose = true;
                continue;
            }

            if (argument is "--jobs" or "-j")
            {
                if (jobs is not null)
                {
                    request = null;
                    error = "Only one --jobs or -j option may be specified.";
                    return false;
                }

                if (index + 1 >= arguments.Count ||
                    !int.TryParse(arguments[++index], NumberStyles.None, CultureInfo.InvariantCulture, out var parsed_jobs) ||
                    parsed_jobs <= 0)
                {
                    request = null;
                    error = $"'{argument}' requires a positive integer value.";
                    return false;
                }

                jobs = parsed_jobs;
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

        request = new FormatRequest(mode ?? FormatMode.All, verbose, jobs ?? DefaultJobs);
        error = null;
        return true;
    }
}
