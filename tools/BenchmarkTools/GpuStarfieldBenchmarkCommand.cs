using System.Globalization;
using System.Text;

namespace BenchmarkTools;

internal static class GpuStarfieldBenchmarkCommand
{
    private static readonly Dictionary<string, string> metrics = new(StringComparer.Ordinal)
    {
        ["game_thread_ms"] = "GameThreadTime",
        ["render_thread_ms"] = "RenderThreadTime",
        ["gpu_ms"] = "GPUTime",
        ["translucency_gpu_ms"] = "GPU/Translucency",
        ["starfield_submit_cpu_ms"] = "Exclusive/AllWorkers/GpuStarfieldSubmit",
        ["rhi_draw_calls"] = "RHI/DrawCalls",
        ["translucency_draw_calls"] = "DrawCall/Translucency",
        ["primitives_drawn"] = "RHI/PrimitivesDrawn",
    };
    private static readonly HashSet<string> value_arguments = ["--editor", "--project", "--output", "--counts", "--resolutions", "--size-multipliers", "--camera-modes", "--warmup-frames", "--capture-frames", "--repeats", "--trim-frames", "--timeout-seconds"];

    internal static Request ParseRequest(IReadOnlyList<string> arguments, RepositoryPaths repository_paths)
    {
        return Request.From(CommandArguments.Parse(arguments, value_arguments, new HashSet<string>(StringComparer.Ordinal)), repository_paths);
    }

    public static async Task<int> RunAsync(BenchmarkToolsApplication application, RepositoryPaths repository_paths, IReadOnlyList<string> arguments, CancellationToken cancellation_token)
    {
        var request = ParseRequest(arguments, repository_paths);
        var command = request.ToArguments();
        return await BenchmarkCommandSupport.RunWithBenchmarkLeaseAsync(
            application, repository_paths, "GPU starfield benchmark", command,
            async () => await RunInsideLeaseAsync(application, repository_paths, request, cancellation_token),
            [BenchmarkCommandSupport.EngineResource(request.Editor)], cancellation_token: cancellation_token);
    }

    private static async Task<int> RunInsideLeaseAsync(BenchmarkToolsApplication application, RepositoryPaths repository_paths, Request request, CancellationToken cancellation_token)
    {
        var run_directory = Path.Combine(request.Output, DateTime.Now.ToString("yyyyMMdd_HHmmss", CultureInfo.InvariantCulture));
        if (Directory.Exists(run_directory))
        {
            throw new BenchmarkToolException($"GPU starfield output directory already exists: '{run_directory}'.");
        }
        Directory.CreateDirectory(run_directory);

        var captures = new List<Capture>();
        foreach (var configuration in request.Configurations)
        {
            var configuration_directory = Path.Combine(run_directory, configuration.Name);
            var raw_directory = Path.Combine(configuration_directory, "raw");
            Directory.CreateDirectory(raw_directory);
            var log_path = Path.Combine(configuration_directory, "unreal.log");
            application.StandardOutput.WriteLine($"Running {configuration.Name}...");
            var result = await application.ProcessRunner.RunAsync(new ProcessRequest(
                request.Editor,
                EditorArguments(request, configuration, raw_directory),
                Path.GetDirectoryName(request.Project)!,
                Timeout: TimeSpan.FromSeconds(request.TimeoutSeconds)), cancellation_token);
            File.WriteAllText(log_path, result.StandardOutput + result.StandardError, new UTF8Encoding(false));
            if (result.ExitCode != 0)
            {
                var tail = string.Join(Environment.NewLine, (result.StandardOutput + result.StandardError).Split(["\r\n", "\n"], StringSplitOptions.None).TakeLast(80));
                throw new BenchmarkToolException($"Unreal benchmark exited with code {result.ExitCode}.{Environment.NewLine}{tail}");
            }
            captures.AddRange(LoadCaptures(raw_directory, configuration, request));
        }

        var deltas = CalculateDeltas(captures, request);
        Validate(captures, deltas, request);
        WriteOutputs(run_directory, request, captures, deltas);
        var report = WriteMarkdown(Path.Combine(run_directory, "GpuStarfieldBenchmark.md"), deltas, request);
        File.WriteAllText(Path.Combine(request.Output, "latest.txt"), run_directory, new UTF8Encoding(false));
        application.StandardOutput.WriteLine(report);
        application.StandardOutput.WriteLine($"Artifacts: {run_directory}");
        return 0;
    }

    internal static IReadOnlyList<string> EditorArguments(Request request, Configuration configuration, string raw_directory)
    {
        return [request.Project, "/SandboxShaders/Showcase/SandboxShaders_Showcase", "-game", "-RenderOffscreen", "-unattended", "-nop4", "-nosplash", "-nosound", "-stdout",
            $"-ResX={configuration.Width}", $"-ResY={configuration.Height}", "-ForceRes", "-windowed", "-benchmark", "-deterministic", "-fps=60", "-csvGpuStats", "-GpuStarfieldBenchmark",
            $"-GpuStarfieldBenchmarkCounts={string.Join(',', request.Counts)}", $"-GpuStarfieldBenchmarkStarSizeMultiplier={configuration.SizeMultiplier.ToString("R", CultureInfo.InvariantCulture)}",
            $"-GpuStarfieldBenchmarkCameraModes={string.Join(',', request.CameraModes.Select(mode => mode ? "moving" : "stationary"))}", $"-GpuStarfieldBenchmarkWarmupFrames={request.WarmupFrames}",
            $"-GpuStarfieldBenchmarkCaptureFrames={request.CaptureFrames}", $"-GpuStarfieldBenchmarkRepeats={request.Repeats}", $"-GpuStarfieldBenchmarkOutput={raw_directory}",
            "-ExecCmds=r.VSync 0;r.ScreenPercentage 100;r.DynamicRes.OperationMode 0"];
    }

    private static IEnumerable<Capture> LoadCaptures(string raw_directory, Configuration configuration, Request request)
    {
        foreach (var count in request.Counts)
            foreach (var repeat in Enumerable.Range(1, request.Repeats))
                foreach (var moving in request.CameraModes)
                    foreach (var enabled in new[] { false, true })
                    {
                        var path = Path.Combine(raw_directory, $"gpu_starfield_{count}_{(moving ? "moving" : "stationary")}_{(enabled ? "enabled" : "disabled")}_r{repeat}.csv");
                        if (!File.Exists(path))
                        {
                            throw new BenchmarkToolException($"Expected capture was not produced: '{path}'.");
                        }
                        yield return ReadCapture(path, configuration, count, repeat, enabled, moving, request.TrimFrames);
                    }
    }

    internal static Capture ReadCapture(string path, Configuration configuration, int count, int repeat, bool enabled, bool moving, int trim_frames)
    {
        using var reader = new StreamReader(path, Encoding.UTF8, detectEncodingFromByteOrderMarks: true);
        var header = reader.ReadLine() ?? throw new BenchmarkToolException($"Capture is empty: '{path}'.");
        var columns = ParseCsv(header);
        var indices = metrics.Where(metric => columns.Contains(metric.Value)).ToDictionary(metric => metric.Key, metric => columns.IndexOf(metric.Value), StringComparer.Ordinal);
        if (!indices.ContainsKey("game_thread_ms"))
        {
            throw new BenchmarkToolException($"Capture has no GameThreadTime column: '{path}'.");
        }
        var values = metrics.Keys.ToDictionary(key => key, _ => new List<double>(), StringComparer.Ordinal);
        var width = 0;
        var height = 0;
        while (reader.ReadLine() is { } line)
        {
            var row = ParseCsv(line);
            ReadResolution(row, ref width, ref height);
            if (!TryRead(row, indices["game_thread_ms"], out _))
            {
                continue;
            }
            foreach (var (name, index) in indices)
            {
                if (TryRead(row, index, out var value))
                {
                    values[name].Add(value);
                }
            }
        }
        var frame_count = values["game_thread_ms"].Count;
        if (frame_count <= trim_frames * 2)
        {
            throw new BenchmarkToolException($"Capture has only {frame_count} frames: '{path}'.");
        }
        if (width != configuration.Width || height != configuration.Height)
        {
            throw new BenchmarkToolException($"Capture rendered at {width}x{height}, expected {configuration.Width}x{configuration.Height}: '{path}'.");
        }
        var medians = new Dictionary<string, double>(StringComparer.Ordinal);
        var minima = new Dictionary<string, double>(StringComparer.Ordinal);
        var maxima = new Dictionary<string, double>(StringComparer.Ordinal);
        foreach (var name in metrics.Keys)
        {
            var samples = values[name].Skip(trim_frames).Take(Math.Max(0, values[name].Count - trim_frames * 2)).Order().ToArray();
            medians[name] = samples.Length == 0 ? 0 : Median(samples);
            minima[name] = samples.Length == 0 ? 0 : samples[0];
            maxima[name] = samples.Length == 0 ? 0 : samples[^1];
        }
        return new Capture(configuration, count, repeat, enabled, moving, frame_count, medians, minima, maxima);
    }

    internal static List<Delta> CalculateDeltas(IReadOnlyList<Capture> captures, Request request)
    {
        var map = new Dictionary<(Configuration Configuration, int StarCount, int Repeat, bool Enabled, bool Moving), Capture>();
        foreach (var capture in captures)
        {
            if (!map.TryAdd((capture.Configuration, capture.StarCount, capture.Repeat, capture.Enabled, capture.Moving), capture))
            {
                throw new BenchmarkToolException("GPU starfield captures contained duplicate configurations.");
            }
        }
        var result = new List<Delta>();
        foreach (var configuration in request.Configurations)
            foreach (var count in request.Counts)
                foreach (var moving in request.CameraModes)
                    foreach (var metric in metrics.Keys)
                    {
                        var disabled = Enumerable.Range(1, request.Repeats).Select(repeat => CaptureMetric(map, configuration, count, repeat, false, moving, metric)).ToArray();
                        var enabled = Enumerable.Range(1, request.Repeats).Select(repeat => CaptureMetric(map, configuration, count, repeat, true, moving, metric)).ToArray();
                        var values = enabled.Zip(disabled).Select(pair => pair.First - pair.Second).Order().ToArray();
                        result.Add(new Delta(configuration, count, moving, metric, Median(disabled.Order()), Median(enabled.Order()), Median(values), values[0], values[^1]));
                    }
        return result;
    }

    internal static void Validate(IReadOnlyList<Capture> captures, IReadOnlyList<Delta> deltas, Request request)
    {
        var lookup = DeltaLookup(deltas);
        var errors = new List<string>();
        foreach (var configuration in request.Configurations)
            foreach (var count in request.Counts)
                foreach (var moving in request.CameraModes)
                {
                    var label = $"{configuration.Name}, {count:N0} {(moving ? "moving" : "stationary")} stars";
                    var draws = DeltaFor(lookup, configuration, count, moving, "translucency_draw_calls").DeltaMedian;
                    var primitives = DeltaFor(lookup, configuration, count, moving, "primitives_drawn").DeltaMedian;
                    var expected = count * 2 + 2;
                    if (draws is < 1.5 or > 2.5) errors.Add($"{label} changed translucency draws by {draws:F2}, expected 2");
                    if (Math.Abs(primitives - expected) > Math.Max(expected * .02, 4)) errors.Add($"{label} changed primitives by {primitives:F0}, expected {expected:N0}");
                    if (DeltaFor(lookup, configuration, count, moving, "starfield_submit_cpu_ms").EnabledMedian <= 0) errors.Add($"{label} did not record the mesh submission scope");
                }
        foreach (var capture in captures.Where(capture => capture.Moving && capture.Enabled))
        {
            var expected = capture.StarCount * 2 + 2;
            if (CaptureMinimum(capture, "primitives_drawn") < expected) errors.Add($"{capture.Configuration.Name}, {capture.StarCount:N0} moving stars disappeared during repeat {capture.Repeat}");
        }
        if (errors.Count > 0) throw new BenchmarkToolException("Benchmark validation failed:" + Environment.NewLine + string.Join(Environment.NewLine, errors.Select(error => "- " + error)));
    }

    private static IEnumerable<string[]> DeltaRows(IEnumerable<Delta> deltas)
    {
        yield return ["width", "height", "star_size_multiplier", "star_count", "camera_motion", "metric", "disabled_median", "enabled_median", "delta_median", "delta_min", "delta_max"];
        foreach (var delta in deltas)
            yield return [delta.Configuration.Width.ToString(CultureInfo.InvariantCulture), delta.Configuration.Height.ToString(CultureInfo.InvariantCulture), delta.Configuration.SizeMultiplier.ToString("G", CultureInfo.InvariantCulture), delta.StarCount.ToString(CultureInfo.InvariantCulture), delta.Moving ? "moving" : "stationary", delta.Metric, delta.DisabledMedian.ToString("F6", CultureInfo.InvariantCulture), delta.EnabledMedian.ToString("F6", CultureInfo.InvariantCulture), delta.DeltaMedian.ToString("F6", CultureInfo.InvariantCulture), delta.DeltaMin.ToString("F6", CultureInfo.InvariantCulture), delta.DeltaMax.ToString("F6", CultureInfo.InvariantCulture)];
    }

    internal static void WriteOutputs(string run_directory, Request request, IReadOnlyList<Capture> captures, IReadOnlyList<Delta> deltas)
    {
        BenchmarkCommandSupport.WriteJson(Path.Combine(run_directory, "GpuStarfieldBenchmark.json"), new OutputDocument(1, request, captures, deltas));
        BenchmarkCommandSupport.WriteCsv(Path.Combine(run_directory, "GpuStarfieldBenchmark.csv"), DeltaRows(deltas));
    }

    private static string WriteMarkdown(string path, IReadOnlyList<Delta> deltas, Request request)
    {
        var report = RenderMarkdown(deltas, request);
        File.WriteAllText(path, report, new UTF8Encoding(false));
        return report;
    }

    internal static string RenderMarkdown(IReadOnlyList<Delta> deltas, Request request)
    {
        var lookup = DeltaLookup(deltas);
        var lines = new List<string>
        {
            "# GPU starfield isolated A/B benchmark", "",
            "Values are medians of paired enabled-minus-disabled captures. The range is the minimum",
            "and maximum paired delta across repeats. CPU totals remain whole-frame deltas; the",
            "starfield submission scope measures only `GetDynamicMeshElements`, which UE schedules on a worker.",
        };
        if (request.CameraModes.Contains(true))
        {
            lines.Add("Moving captures translate the camera 10,000 km along a deterministic curved path.");
        }
        lines.AddRange(["", "| Resolution | Size | Stars | Camera | GT delta ms | RT delta ms | Submit CPU ms | GPU delta ms | Translucency GPU delta ms | Draw delta | Primitive delta |", "| :--- | ---: | ---: | :--- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |"]);
        foreach (var configuration in request.Configurations)
            foreach (var count in request.Counts)
                foreach (var moving in request.CameraModes)
                {
                    var item = (configuration, count, moving);
                    lines.Add($"| {configuration.Width}x{configuration.Height} | {configuration.SizeMultiplier:G} | {count:N0} | {(moving ? "Moving" : "Stationary")} | {DeltaFor(lookup, item.configuration, item.count, item.moving, "game_thread_ms").DeltaMedian:F4} | {DeltaFor(lookup, item.configuration, item.count, item.moving, "render_thread_ms").DeltaMedian:F4} | {DeltaFor(lookup, item.configuration, item.count, item.moving, "starfield_submit_cpu_ms").EnabledMedian:F4} | {DeltaFor(lookup, item.configuration, item.count, item.moving, "gpu_ms").DeltaMedian:F4} | {DeltaFor(lookup, item.configuration, item.count, item.moving, "translucency_gpu_ms").DeltaMedian:F4} | {DeltaFor(lookup, item.configuration, item.count, item.moving, "translucency_draw_calls").DeltaMedian:F1} | {DeltaFor(lookup, item.configuration, item.count, item.moving, "primitives_drawn").DeltaMedian:F0} |");
                }
        lines.AddRange(["", "Paired delta ranges:", ""]);
        foreach (var configuration in request.Configurations)
            foreach (var count in request.Counts)
                foreach (var moving in request.CameraModes)
                {
                    lines.Add($"- {configuration.Width}x{configuration.Height}, size {configuration.SizeMultiplier:G}, {count:N0} stars, {(moving ? "moving" : "stationary")} camera:");
                    foreach (var metric in new[] { "game_thread_ms", "render_thread_ms", "gpu_ms", "translucency_gpu_ms" })
                    {
                        var delta = DeltaFor(lookup, configuration, count, moving, metric);
                        lines.Add($"  - `{metric}`: {delta.DeltaMedian:F4} ms [{delta.DeltaMin:F4}, {delta.DeltaMax:F4}]");
                    }
                }
        return string.Join(Environment.NewLine, lines) + Environment.NewLine;
    }

    internal static List<string> ParseCsv(string line)
    {
        var result = new List<string>(); var builder = new StringBuilder(); var quoted = false;
        for (var index = 0; index < line.Length; ++index) { var character = line[index]; if (character == '"' && quoted && index + 1 < line.Length && line[index + 1] == '"') { builder.Append(character); ++index; } else if (character == '"') quoted = !quoted; else if (character == ',' && !quoted) { result.Add(builder.ToString()); builder.Clear(); } else builder.Append(character); }
        if (quoted) throw new BenchmarkToolException("GPU starfield CSV contains an unterminated quoted field.");
        result.Add(builder.ToString()); return result;
    }
    private static bool TryRead(IReadOnlyList<string> row, int index, out double value)
    {
        value = 0;
        return index < row.Count && double.TryParse(row[index], NumberStyles.Float, CultureInfo.InvariantCulture, out value) && double.IsFinite(value);
    }
    private static void ReadResolution(IReadOnlyList<string> row, ref int width, ref int height) { for (var index = 0; index + 1 < row.Count; ++index) { if (row[index] == "[systemresolution.resx]" && int.TryParse(row[index + 1], out var value)) width = value; if (row[index] == "[systemresolution.resy]" && int.TryParse(row[index + 1], out var value2)) height = value2; } }
    internal static double Median(IEnumerable<double> values) { var data = values.Order().ToArray(); if (data.Length == 0) throw new BenchmarkToolException("Cannot calculate a median from no GPU starfield samples."); return data.Length % 2 == 1 ? data[data.Length / 2] : (data[data.Length / 2 - 1] + data[data.Length / 2]) / 2; }

    private static double CaptureMetric(Capture capture, string metric)
    {
        if (!capture.Medians.TryGetValue(metric, out var value) || !double.IsFinite(value))
        {
            throw new BenchmarkToolException($"GPU starfield capture did not contain a finite '{metric}' value.");
        }
        return value;
    }

    private static double CaptureMinimum(Capture capture, string metric)
    {
        if (!capture.Minima.TryGetValue(metric, out var value) || !double.IsFinite(value))
        {
            throw new BenchmarkToolException($"GPU starfield capture did not contain a finite minimum '{metric}' value.");
        }
        return value;
    }

    private static double CaptureMetric(
        IReadOnlyDictionary<(Configuration Configuration, int StarCount, int Repeat, bool Enabled, bool Moving), Capture> captures,
        Configuration configuration, int count, int repeat, bool enabled, bool moving, string metric)
    {
        if (!captures.TryGetValue((configuration, count, repeat, enabled, moving), out var capture))
        {
            throw new BenchmarkToolException($"GPU starfield capture is missing for {configuration.Name}, {count:N0} stars, repeat {repeat}.");
        }
        return CaptureMetric(capture, metric);
    }

    private static Dictionary<(Configuration Configuration, int StarCount, bool Moving, string Metric), Delta> DeltaLookup(IReadOnlyList<Delta> deltas)
    {
        var result = new Dictionary<(Configuration Configuration, int StarCount, bool Moving, string Metric), Delta>();
        foreach (var delta in deltas)
        {
            if (!result.TryAdd((delta.Configuration, delta.StarCount, delta.Moving, delta.Metric), delta))
            {
                throw new BenchmarkToolException("GPU starfield results contained duplicate deltas.");
            }
        }
        return result;
    }

    private static Delta DeltaFor(
        IReadOnlyDictionary<(Configuration Configuration, int StarCount, bool Moving, string Metric), Delta> deltas,
        Configuration configuration, int count, bool moving, string metric)
    {
        if (!deltas.TryGetValue((configuration, count, moving, metric), out var delta))
        {
            throw new BenchmarkToolException($"GPU starfield result is missing '{metric}' for {configuration.Name}, {count:N0} stars.");
        }
        return delta;
    }

    internal sealed record Configuration(int Width, int Height, double SizeMultiplier) { public string Name => $"{Width}x{Height}_size{SizeMultiplier.ToString("G", CultureInfo.InvariantCulture).Replace('.', 'p')}"; }
    internal sealed record Capture(Configuration Configuration, int StarCount, int Repeat, bool Enabled, bool Moving, int FrameCount, IReadOnlyDictionary<string, double> Medians, IReadOnlyDictionary<string, double> Minima, IReadOnlyDictionary<string, double> Maxima);
    internal sealed record Delta(Configuration Configuration, int StarCount, bool Moving, string Metric, double DisabledMedian, double EnabledMedian, double DeltaMedian, double DeltaMin, double DeltaMax);
    private sealed record OutputDocument(int SchemaVersion, Request Request, IReadOnlyList<Capture> Captures, IReadOnlyList<Delta> Deltas);
    internal sealed record Request(string Editor, string Project, string Output, IReadOnlyList<int> Counts, IReadOnlyList<Configuration> Configurations, IReadOnlyList<bool> CameraModes, int WarmupFrames, int CaptureFrames, int Repeats, int TrimFrames, int TimeoutSeconds)
    {
        public static Request From(CommandArguments parsed, RepositoryPaths paths)
        {
            var counts = ParseInts(parsed.Value("--counts", "10000,100000,1000000"), 1, 1_000_000, "--counts");
            var resolutions = parsed.Value("--resolutions", "1280x720").Split(',', StringSplitOptions.None).Select(item => item.Trim().Split('x', 'X')).Select(parts => parts.Length == 2 && int.TryParse(parts[0], out var width) && int.TryParse(parts[1], out var height) && width is >= 320 and <= 7680 && height is >= 200 and <= 4320 ? (width, height) : throw new BenchmarkToolException("'--resolutions' must use unique WIDTHxHEIGHT entries between 320x200 and 7680x4320.")).Distinct().ToArray();
            var sizes = parsed.Value("--size-multipliers", "1").Split(',', StringSplitOptions.None).Select(item => double.TryParse(item.Trim(), NumberStyles.Float, CultureInfo.InvariantCulture, out var value) && double.IsFinite(value) && value is >= 0 and <= 100 ? value : throw new BenchmarkToolException("'--size-multipliers' must contain finite values from 0 to 100.")).Distinct().ToArray();
            var modes = parsed.Value("--camera-modes", "stationary,moving").Split(',', StringSplitOptions.None).Select(item => item.Trim().ToLowerInvariant() switch { "stationary" => false, "moving" => true, _ => throw new BenchmarkToolException("'--camera-modes' must contain stationary and/or moving.") }).Distinct().ToArray();
            if (resolutions.Length == 0 || sizes.Length == 0 || modes.Length == 0) throw new BenchmarkToolException("GPU starfield lists cannot be empty.");
            return new Request(Path.GetFullPath(parsed.Required("--editor")), Path.GetFullPath(parsed.Required("--project")), BenchmarkCommandSupport.ResolveOutputDirectory(paths, parsed.Required("--output")), counts, resolutions.SelectMany(resolution => sizes.Select(size => new Configuration(resolution.width, resolution.height, size))).ToArray(), modes, parsed.PositiveInt32("--warmup-frames", 60), parsed.PositiveInt32("--capture-frames", 180), parsed.PositiveInt32("--repeats", 3), parsed.PositiveInt32("--trim-frames", 10), parsed.PositiveInt32("--timeout-seconds", 1800));
        }
        public IReadOnlyList<string> ToArguments() => ["gpu-starfield", "--editor", Editor, "--project", Project, "--output", Output, "--counts", string.Join(',', Counts), "--resolutions", string.Join(',', Configurations.Select(configuration => $"{configuration.Width}x{configuration.Height}").Distinct()), "--size-multipliers", string.Join(',', Configurations.Select(configuration => configuration.SizeMultiplier.ToString("R", CultureInfo.InvariantCulture)).Distinct()), "--camera-modes", string.Join(',', CameraModes.Select(mode => mode ? "moving" : "stationary")), "--warmup-frames", WarmupFrames.ToString(CultureInfo.InvariantCulture), "--capture-frames", CaptureFrames.ToString(CultureInfo.InvariantCulture), "--repeats", Repeats.ToString(CultureInfo.InvariantCulture), "--trim-frames", TrimFrames.ToString(CultureInfo.InvariantCulture), "--timeout-seconds", TimeoutSeconds.ToString(CultureInfo.InvariantCulture)];
        private static int[] ParseInts(string value, int minimum, int maximum, string name) { var values = new List<int>(); var seen = new HashSet<int>(); foreach (var item in value.Split(',', StringSplitOptions.None)) { if (!int.TryParse(item.Trim(), NumberStyles.None, CultureInfo.InvariantCulture, out var number) || number < minimum || number > maximum || !seen.Add(number)) throw new BenchmarkToolException($"'{name}' must contain unique values in the range {minimum}..{maximum}."); values.Add(number); } if (values.Count == 0) throw new BenchmarkToolException($"'{name}' cannot be empty."); return [.. values]; }
    }
}
