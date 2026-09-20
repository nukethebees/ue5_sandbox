using System.Globalization;
using System.Text.Json;

namespace BenchmarkTools;

internal static class FighterSimulationBenchmarkCommand
{
    private static readonly HashSet<string> value_arguments = ["--fighter-caps", "--seconds", "--warmup-seconds", "--saturation-timeout-seconds", "--output-dir"];
    private static readonly HashSet<string> flag_arguments = ["--skip-build"];

    public static async Task<int> RunAsync(BenchmarkToolsApplication application, RepositoryPaths repository_paths, IReadOnlyList<string> arguments, CancellationToken cancellation_token)
    {
        var parsed = CommandArguments.Parse(arguments, value_arguments, flag_arguments);
        var caps = ParseCaps(parsed.Value("--fighter-caps", "2000,4000"));
        var seconds = parsed.FiniteDouble("--seconds", 10.0, 0.1, 86400.0);
        var warmup = parsed.FiniteDouble("--warmup-seconds", 5.0, 0.0, 86400.0);
        var saturation_timeout = parsed.FiniteDouble("--saturation-timeout-seconds", 60.0, 0.1, 86400.0);
        var output = BenchmarkCommandSupport.ResolveOutputDirectory(
            repository_paths,
            parsed.Value("--output-dir", $".local/benchmarks/fighter-simulation/{DateTime.Now:yyyyMMdd-HHmmss}"));
        Directory.CreateDirectory(output);

        var command = new List<string>
        {
            "native-simulation", "--level", Path.Combine(repository_paths.Root, "LevelScripts", "FighterSchedulingBenchmark.scm"),
            "--seconds", NativeSimulationBenchmarkRequest.FormatDouble(seconds),
            "--fighter-stress-caps", string.Join(',', caps),
            "--warmup-seconds", NativeSimulationBenchmarkRequest.FormatDouble(warmup),
            "--saturation-timeout-seconds", NativeSimulationBenchmarkRequest.FormatDouble(saturation_timeout),
        };
        if (parsed.HasFlag("--skip-build"))
        {
            command.Add("--skip-build");
        }

        application.StandardOutput.WriteLine($"Running fighter caps {string.Join(", ", caps)} in one native benchmark process...");
        var process = await BenchmarkCommandSupport.RunSelfAsync(application, repository_paths, command, cancellation_token);
        if (process.ExitCode != 0)
        {
            application.WriteProcessOutput(process);
            return process.ExitCode;
        }

        var results = BenchmarkCommandSupport.JsonLines(process.StandardOutput).ToArray();
        if (results.Length != caps.Length)
        {
            throw new BenchmarkToolException($"Expected {caps.Length} benchmark JSON results, found {results.Length}.");
        }
        for (var index = 0; index < caps.Length; ++index)
        {
            Validate(results[index], caps[index], (int)Math.Ceiling(seconds * 60.0));
        }

        BenchmarkCommandSupport.WriteJson(Path.Combine(output, "results.json"), results);
        BenchmarkCommandSupport.WriteCsv(Path.Combine(output, "summary.csv"), SummaryRows(results, caps));
        application.StandardOutput.WriteLine($"Results written to {output}");
        return 0;
    }

    private static uint[] ParseCaps(string value)
    {
        var caps = new List<uint>();
        var seen = new HashSet<uint>();
        foreach (var item in value.Split(',', StringSplitOptions.None))
        {
            if (!uint.TryParse(item.Trim(), NumberStyles.None, CultureInfo.InvariantCulture, out var cap) || cap == 0 || !seen.Add(cap))
            {
                throw new BenchmarkToolException("'--fighter-caps' must contain unique positive 32-bit integers.");
            }
            caps.Add(cap);
        }
        if (caps.Count == 0)
        {
            throw new BenchmarkToolException("'--fighter-caps' must contain one or more values.");
        }
        return [.. caps];
    }

    private static void Validate(JsonElement result, uint cap, int expected_ticks)
    {
        try
        {
            if (result.GetProperty("level").GetProperty("id").GetString() != "fighter-scheduling-benchmark" ||
                !result.GetProperty("fighter_stress").GetProperty("enabled").GetBoolean() ||
                result.GetProperty("fighter_stress").GetProperty("configured_cap").GetUInt32() != cap ||
                result.GetProperty("workload").GetProperty("measured_ticks").GetInt32() != expected_ticks ||
                result.GetProperty("fighter_stress").GetProperty("steady_state_fighters").GetUInt32() != cap ||
                result.GetProperty("fighter_stress").GetProperty("minimum_measured_fighters").GetUInt32() != cap ||
                result.GetProperty("fighter_stress").GetProperty("maximum_measured_fighters").GetUInt32() != cap ||
                result.GetProperty("fighter_stress").GetProperty("fighter_spawns_during_measurement").GetUInt32() != 0 ||
                result.GetProperty("fighter_stress").GetProperty("task_counts").GetProperty("attack").GetUInt32() != cap ||
                result.GetProperty("fighter_stress").GetProperty("lasers_spawned_during_measurement").GetUInt32() <= 0 ||
                result.GetProperty("memory").GetProperty("frame_overflow_count").GetUInt32() != 0)
            {
                throw new BenchmarkToolException($"Fighter benchmark validation failed for cap {cap}.");
            }
        }
        catch (KeyNotFoundException exception)
        {
            throw new BenchmarkToolException($"Fighter benchmark result for cap {cap} was malformed: {exception.Message}");
        }
    }

    private static IEnumerable<string[]> SummaryRows(IReadOnlyList<JsonElement> results, IReadOnlyList<uint> caps)
    {
        yield return ["fighter_cap", "steady_state_fighters", "measured_ticks", "elapsed_seconds", "mean_tick_us", "median_tick_us", "p95_tick_us", "p99_tick_us", "ticks_per_second", "realtime_factor", "lasers_spawned"];
        for (var index = 0; index < results.Count; ++index)
        {
            var result = results[index];
            yield return [
                caps[index].ToString(CultureInfo.InvariantCulture),
                result.GetProperty("fighter_stress").GetProperty("steady_state_fighters").GetRawText(),
                result.GetProperty("workload").GetProperty("measured_ticks").GetRawText(),
                result.GetProperty("timing").GetProperty("elapsed_seconds").GetRawText(),
                result.GetProperty("timing").GetProperty("mean_tick_microseconds").GetRawText(),
                result.GetProperty("timing").GetProperty("median_tick_microseconds").GetRawText(),
                result.GetProperty("timing").GetProperty("p95_tick_microseconds").GetRawText(),
                result.GetProperty("timing").GetProperty("p99_tick_microseconds").GetRawText(),
                result.GetProperty("timing").GetProperty("ticks_per_second").GetRawText(),
                result.GetProperty("timing").GetProperty("realtime_factor").GetRawText(),
                result.GetProperty("fighter_stress").GetProperty("lasers_spawned_during_measurement").GetRawText(),
            ];
        }
    }
}

internal static class FrameMemoryLevelBenchmarkCommand
{
    private static readonly HashSet<string> value_arguments = ["--seconds"];
    private static readonly HashSet<string> flag_arguments = ["--skip-build"];

    public static async Task<int> RunAsync(BenchmarkToolsApplication application, RepositoryPaths repository_paths, IReadOnlyList<string> arguments, CancellationToken cancellation_token)
    {
        var parsed = CommandArguments.Parse(arguments, value_arguments, flag_arguments);
        var seconds = parsed.FiniteDouble("--seconds", 20.0, 0.1, 86400.0);
        var command = new List<string>
        {
            "native-simulation", "--level", Path.Combine(repository_paths.Root, "LevelScripts", "Benchmarks", "Batch_benchmark.scm"),
            "--seconds", NativeSimulationBenchmarkRequest.FormatDouble(seconds), "--game-speed", "100",
            "--build-preset", "frame-memory-level-benchmark",
        };
        if (parsed.HasFlag("--skip-build"))
        {
            command.Add("--skip-build");
        }

        var process = await BenchmarkCommandSupport.RunSelfAsync(application, repository_paths, command, cancellation_token);
        if (process.ExitCode != 0)
        {
            application.WriteProcessOutput(process);
            return process.ExitCode;
        }
        var results = BenchmarkCommandSupport.JsonLines(process.StandardOutput).ToArray();
        if (results.Length != 1)
        {
            throw new BenchmarkToolException($"Expected one native simulation benchmark JSON result, found {results.Length}.");
        }
        Validate(results[0]);
        application.StandardOutput.WriteLine(results[0].GetRawText());
        return 0;
    }

    private static void Validate(JsonElement result)
    {
        var workload = result.GetProperty("workload");
        var memory = result.GetProperty("memory");
        if (result.GetProperty("level").GetProperty("id").GetString() != "batch-benchmark" ||
            workload.GetProperty("completed_ticks").GetInt32() != workload.GetProperty("requested_ticks").GetInt32() ||
            workload.GetProperty("game_speed").GetInt32() != 100 ||
            workload.GetProperty("advance_calls").GetInt32() != workload.GetProperty("requested_ticks").GetInt32() ||
            memory.GetProperty("frame_overflow_count").GetUInt32() != 0 ||
            memory.GetProperty("frame_peak_claimed_bytes").GetUInt64() == 0 ||
            result.GetProperty("final_state").GetProperty("peak_fighters").GetUInt32() == 0)
        {
            throw new BenchmarkToolException("Frame-memory benchmark validation failed.");
        }
    }
}
