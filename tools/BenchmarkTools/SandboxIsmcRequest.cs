using System.Globalization;

namespace BenchmarkTools;

internal sealed record SandboxIsmcRequest(string Editor, int Width, int Height, int Instances, double UpdatePercent,
    string Mode, string Visibility, string Bounds, string CustomData, bool Shadows, bool Churn, int MinInstances,
    int HalfCycleUpdates, double ReplacementPercent, int WarmupUpdates, double WarmupSeconds, double Seconds, bool Trace)
{
    internal static readonly HashSet<string> ValueArguments = ["--editor", "--width", "--height", "--instances", "--update-percent", "--mode",
        "--visibility", "--bounds", "--custom-data", "--shadows", "--churn", "--min-instances", "--half-cycle-updates", "--replacement-percent",
        "--warmup-updates", "--warmup-seconds", "--seconds", "--trace", "--output-dir", "--baseline", "--baseline-worktree", "--repetitions", "--warmup-runs"];

    public static SandboxIsmcRequest Parse(CommandArguments args, RepositoryPaths repository)
    {
        string Choice(string name, string fallback, params string[] choices)
        {
            var value = args.Value(name, fallback);
            if (!choices.Contains(value)) throw new BenchmarkToolException($"{name} must be one of {string.Join(", ", choices)}.");
            return value;
        }
        int Nonnegative(string name, int fallback, int maximum) => args.NonnegativeInt32(name, fallback, maximum);
        var instances = args.PositiveInt32("--instances", 40000);
        var editor = args.Value("--editor", Path.Combine(Environment.GetEnvironmentVariable("UE_ROOT") ?? string.Empty,
            "Engine", "Binaries", "Win64", "UnrealEditor-Cmd.exe"));
        editor = Path.GetFullPath(editor, repository.Root);
        if (!File.Exists(editor)) throw new BenchmarkToolException($"Unreal Editor executable does not exist: '{editor}'. Use --editor or UE_ROOT.");
        return new SandboxIsmcRequest(editor, args.PositiveInt32("--width", 1280, 16384), args.PositiveInt32("--height", 720, 16384), instances,
            args.FiniteDouble("--update-percent", 100, 0, 100), Choice("--mode", "paired", "paired", "custom", "engine_ismc"),
            Choice("--visibility", "all", "all", "half", "none"), Choice("--bounds", "calculated", "calculated", "supplied"),
            Choice("--custom-data", "none", "none", "static", "animated"), Choice("--shadows", "0", "0", "1") == "1",
            Choice("--churn", "0", "0", "1") == "1", Nonnegative("--min-instances", Math.Min(1000, instances), instances),
            args.PositiveInt32("--half-cycle-updates", 120), args.FiniteDouble("--replacement-percent", 5, 0, 100),
            Nonnegative("--warmup-updates", 0, int.MaxValue), args.FiniteDouble("--warmup-seconds", 1, 0, 3600),
            args.FiniteDouble("--seconds", 5, .01, 3600), Choice("--trace", "1", "0", "1") == "1");
    }

    public Dictionary<string, string> Conditions() => new(StringComparer.Ordinal)
    {
        ["mode"] = Mode, ["visibility"] = Visibility + "_visible", ["bounds"] = Bounds,
        ["custom_data"] = CustomData == "none" ? "no_custom_data" : CustomData + "_rgb",
        ["instances"] = Number(Instances), ["update_percent"] = Number(UpdatePercent), ["shadows"] = Shadows ? "1" : "0",
        ["churn"] = Churn ? "1" : "0", ["min_instances"] = Number(MinInstances), ["half_cycle_updates"] = Number(HalfCycleUpdates),
        ["replacement_percent"] = Number(ReplacementPercent), ["warmup_updates"] = Number(WarmupUpdates),
        ["warmup_seconds"] = Number(WarmupSeconds), ["measurement_seconds"] = Number(Seconds), ["trace"] = Trace ? "1" : "0",
        ["requested_width"] = Number(Width), ["requested_height"] = Number(Height), ["observed_width"] = Number(Width), ["observed_height"] = Number(Height),
    };

    public IReadOnlyList<string> EditorArguments(string root, BenchmarkRunContext run)
    {
        return [Path.Combine(root, "Sandbox.uproject"), "-unattended", "-nop4", "-nosplash", "-nosound", "-stdout", "-FullStdOutLogOutput", "-RenderOffscreen",
            "-ddc=NoZenLocalFallback", $"-LocalDataCachePath={Path.Combine(root, "out", "build", "sandbox-ismc-benchmark", "local-derived-data-cache")}",
            "-ExecCmds=r.VSync 0;r.ScreenPercentage 100;r.DynamicRes.OperationMode 0;Automation Now;RunTests SandboxISMC.RemoteBenchmark;Quit",
            "-SandboxISMCBenchmarkEndPIE", $"-abslog={run.Artifact("unreal.log")}",
            $"-ResX={Width}", $"-ResY={Height}", "-ForceRes", "-windowed",
            $"-SandboxISMCBenchmarkOutput={run.DirectoryPath}", $"-SandboxISMCBenchmarkRunId={run.Manifest.RunId}",
            $"-SandboxISMCBenchmarkWidth={Width}", $"-SandboxISMCBenchmarkHeight={Height}",
            $"-SandboxISMCBenchmarkInstances={Instances}", $"-SandboxISMCBenchmarkUpdatePercent={Number(UpdatePercent)}",
            $"-SandboxISMCBenchmarkMode={Mode}", $"-SandboxISMCBenchmarkVisibility={Visibility}", $"-SandboxISMCBenchmarkBounds={Bounds}",
            $"-SandboxISMCBenchmarkCustomData={CustomData}", $"-SandboxISMCBenchmarkShadows={(Shadows ? 1 : 0)}", $"-SandboxISMCBenchmarkChurn={(Churn ? 1 : 0)}",
            $"-SandboxISMCBenchmarkMinInstances={MinInstances}", $"-SandboxISMCBenchmarkHalfCycleUpdates={HalfCycleUpdates}",
            $"-SandboxISMCBenchmarkReplacementPercent={Number(ReplacementPercent)}", $"-SandboxISMCBenchmarkWarmupUpdates={WarmupUpdates}",
            $"-SandboxISMCBenchmarkWarmupSeconds={Number(WarmupSeconds)}", $"-SandboxISMCBenchmarkSeconds={Number(Seconds)}", $"-SandboxISMCBenchmarkTrace={(Trace ? 1 : 0)}"];
    }

    private static string Number(double value) => value.ToString("G9", CultureInfo.InvariantCulture);
}
