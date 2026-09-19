using System.Globalization;

namespace BenchmarkTools;

internal sealed record NativeSimulationBenchmarkRequest(
    string LevelPath,
    double Seconds,
    uint GameSpeed,
    bool Telemetry,
    uint? FighterStressCap,
    IReadOnlyList<uint> FighterStressCaps,
    double WarmupSeconds,
    double SaturationTimeoutSeconds,
    string BuildPreset,
    bool SkipBuild)
{
    public bool HasFighterStress => FighterStressCap.HasValue || FighterStressCaps.Count > 0;

    public IReadOnlyList<string> ToCommandArguments(bool include_skip_build)
    {
        var arguments = new List<string>
        {
            "native-simulation",
            "--level",
            LevelPath,
            "--seconds",
            FormatDouble(Seconds),
            "--game-speed",
            GameSpeed.ToString(CultureInfo.InvariantCulture),
            "--build-preset",
            BuildPreset,
        };
        if (Telemetry)
        {
            arguments.Add("--telemetry");
        }
        if (FighterStressCap.HasValue)
        {
            arguments.Add("--fighter-stress-cap");
            arguments.Add(FighterStressCap.Value.ToString(CultureInfo.InvariantCulture));
        }
        if (FighterStressCaps.Count > 0)
        {
            arguments.Add("--fighter-stress-caps");
            arguments.Add(string.Join(',', FighterStressCaps.Select(cap => cap.ToString(CultureInfo.InvariantCulture))));
        }
        if (HasFighterStress)
        {
            arguments.Add("--warmup-seconds");
            arguments.Add(FormatDouble(WarmupSeconds));
            arguments.Add("--saturation-timeout-seconds");
            arguments.Add(FormatDouble(SaturationTimeoutSeconds));
        }
        if (include_skip_build || SkipBuild)
        {
            arguments.Add("--skip-build");
        }

        return arguments;
    }

    public IReadOnlyList<string> ToBenchmarkArguments()
    {
        var arguments = new List<string>
        {
            "--level",
            LevelPath,
            "--seconds",
            FormatDouble(Seconds),
            "--game-speed",
            GameSpeed.ToString(CultureInfo.InvariantCulture),
        };
        if (Telemetry)
        {
            arguments.Add("--telemetry");
        }
        if (FighterStressCap.HasValue)
        {
            arguments.Add("--fighter-stress-cap");
            arguments.Add(FighterStressCap.Value.ToString(CultureInfo.InvariantCulture));
        }
        if (FighterStressCaps.Count > 0)
        {
            arguments.Add("--fighter-stress-caps");
            foreach (var cap in FighterStressCaps)
            {
                arguments.Add(cap.ToString(CultureInfo.InvariantCulture));
            }
        }
        if (HasFighterStress)
        {
            arguments.Add("--warmup-seconds");
            arguments.Add(FormatDouble(WarmupSeconds));
            arguments.Add("--saturation-timeout-seconds");
            arguments.Add(FormatDouble(SaturationTimeoutSeconds));
        }

        return arguments;
    }

    public static string FormatDouble(double value)
    {
        return value.ToString("R", CultureInfo.InvariantCulture);
    }
}
