using System.Globalization;

namespace BenchmarkTools;

internal static class NativeSimulationBenchmarkCommand
{
    public static bool TryParse(
        IReadOnlyList<string> arguments,
        out NativeSimulationBenchmarkRequest? request,
        out string error)
    {
        ArgumentNullException.ThrowIfNull(arguments);

        request = null;
        error = "Invalid native-simulation arguments.";
        var values = new Dictionary<string, string>(StringComparer.Ordinal);
        var flags = new HashSet<string>(StringComparer.Ordinal);
        for (var index = 0; index < arguments.Count; ++index)
        {
            var argument = arguments[index];
            if (argument is "--telemetry" or "--skip-build")
            {
                if (!flags.Add(argument))
                {
                    error = $"Duplicate argument '{argument}'.";
                    return false;
                }

                continue;
            }
            if (argument is not ("--level" or "--seconds" or "--game-speed" or "--fighter-stress-cap" or
                                 "--fighter-stress-caps" or "--warmup-seconds" or "--saturation-timeout-seconds" or
                                 "--build-preset"))
            {
                error = $"Unknown argument '{argument}'.";
                return false;
            }
            if (index + 1 >= arguments.Count || string.IsNullOrWhiteSpace(arguments[index + 1]) ||
                !values.TryAdd(argument, arguments[++index]))
            {
                error = $"Missing or duplicate value for '{argument}'.";
                return false;
            }
        }

        if (!values.TryGetValue("--level", out var level_path) ||
            !values.TryGetValue("--seconds", out var seconds_text))
        {
            error = "--level and --seconds are required.";
            return false;
        }
        if (!TryParseFiniteDouble(seconds_text, 0.0, double.PositiveInfinity, "--seconds", out var seconds, out error) ||
            !TryParsePositiveUInt32(values.GetValueOrDefault("--game-speed", "1"), "--game-speed", out var game_speed, out error) ||
            !TryParseFiniteDouble(values.GetValueOrDefault("--warmup-seconds", "5"), 0.0, 86400.0, "--warmup-seconds", out var warmup_seconds, out error) ||
            !TryParseFiniteDouble(values.GetValueOrDefault("--saturation-timeout-seconds", "60"), 0.1, 86400.0, "--saturation-timeout-seconds", out var saturation_timeout_seconds, out error))
        {
            return false;
        }

        uint? fighter_stress_cap = null;
        if (values.TryGetValue("--fighter-stress-cap", out var fighter_stress_cap_text))
        {
            if (!TryParsePositiveUInt32(fighter_stress_cap_text, "--fighter-stress-cap", out var cap, out error))
            {
                return false;
            }

            fighter_stress_cap = cap;
        }

        var fighter_stress_caps = Array.Empty<uint>();
        if (values.TryGetValue("--fighter-stress-caps", out var fighter_stress_caps_text) &&
            !TryParseFighterStressCaps(fighter_stress_caps_text, "--fighter-stress-caps", out fighter_stress_caps, out error))
        {
            return false;
        }
        if (fighter_stress_cap.HasValue && fighter_stress_caps.Length > 0)
        {
            error = "--fighter-stress-cap and --fighter-stress-caps are mutually exclusive.";
            return false;
        }
        request = new NativeSimulationBenchmarkRequest(
            Path.GetFullPath(level_path),
            seconds,
            game_speed,
            flags.Contains("--telemetry"),
            fighter_stress_cap,
            fighter_stress_caps,
            warmup_seconds,
            saturation_timeout_seconds,
            values.GetValueOrDefault("--build-preset", "native-simulation-benchmark"),
            flags.Contains("--skip-build"));
        return true;
    }

    private static bool TryParseFighterStressCaps(string value, string argument, out uint[] caps, out string error)
    {
        var result = new List<uint>();
        var unique_caps = new HashSet<uint>();
        foreach (var cap_text in value.Split(',', StringSplitOptions.None))
        {
            if (!TryParsePositiveUInt32(cap_text.Trim(), argument, out var cap, out error) || !unique_caps.Add(cap))
            {
                caps = [];
                error = $"{argument} must contain unique positive comma-separated 32-bit integers.";
                return false;
            }

            result.Add(cap);
        }

        caps = [.. result];
        error = string.Empty;
        return true;
    }

    private static bool TryParsePositiveUInt32(string value, string argument, out uint number, out string error)
    {
        if (!uint.TryParse(value, NumberStyles.None, CultureInfo.InvariantCulture, out number) || number == 0)
        {
            error = $"{argument} must be a positive 32-bit integer.";
            return false;
        }

        error = string.Empty;
        return true;
    }

    private static bool TryParseFiniteDouble(
        string value,
        double minimum,
        double maximum,
        string argument,
        out double number,
        out string error)
    {
        if (!double.TryParse(value, NumberStyles.Float, CultureInfo.InvariantCulture, out number) ||
            !double.IsFinite(number) || number < minimum || number > maximum ||
            (minimum == 0.0 && number == 0.0 && argument == "--seconds"))
        {
            error = $"{argument} must be finite and in the range {minimum.ToString(CultureInfo.InvariantCulture)} to {maximum.ToString(CultureInfo.InvariantCulture)}.";
            return false;
        }

        error = string.Empty;
        return true;
    }
}
