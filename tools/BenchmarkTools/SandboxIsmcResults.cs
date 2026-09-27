using System.Globalization;
using System.Text;
using System.Text.Json;

namespace BenchmarkTools;

internal sealed record SandboxIsmcCapture(string RunId, string Directory, BenchmarkRepetition Repetition,
    IReadOnlyDictionary<string, string> Conditions, IReadOnlyList<BenchmarkMetric> Metrics);

internal static class SandboxIsmcResults
{
    internal static readonly string[] RequiredConditions = ["result_schema", "mode", "visibility", "bounds", "custom_data", "rhi", "instances", "update_percent",
        "churn", "min_instances", "half_cycle_updates", "replacement_percent", "warmup_updates", "warmup_seconds", "measurement_seconds", "shadows", "trace",
        "requested_width", "requested_height", "observed_width", "observed_height", "grid_spacing", "grid_gap", "movement_amplitude", "movement_frequency", "rotation_speed",
        "frame_limits_disabled", "r.ScreenPercentage", "r.DynamicRes.OperationMode", "r.VSync"];

    public static SandboxIsmcCapture Read(BenchmarkRunContext run, SandboxIsmcRequest request, BenchmarkRepetition repetition)
    {
        try { return ReadCapture(run, request, repetition); }
        catch (Exception error) when (error is KeyNotFoundException or InvalidOperationException or FormatException)
        {
            throw new BenchmarkToolException($"Malformed SandboxISMC result: {error.Message}");
        }
    }

    private static SandboxIsmcCapture ReadCapture(BenchmarkRunContext run, SandboxIsmcRequest request, BenchmarkRepetition repetition)
    {
        using var json = JsonDocument.Parse(File.ReadAllText(run.Artifact("result.json")));
        var result = json.RootElement;
        if (result.GetProperty("schemaVersion").GetInt32() != 1 || result.GetProperty("runId").GetString() != run.Manifest.RunId)
            throw new BenchmarkToolException("SandboxISMC result schema/run identity mismatch.");
        var conditions = result.GetProperty("conditions").EnumerateObject().ToDictionary(item => item.Name, item => item.Value.GetString()!, StringComparer.Ordinal);
        foreach (var key in RequiredConditions)
            if (!conditions.TryGetValue(key, out var value) || string.IsNullOrWhiteSpace(value)) throw new BenchmarkToolException($"Missing comparability condition: {key}");
        if (conditions["result_schema"] != "1") throw new BenchmarkToolException("Unsupported SandboxISMC conditions schema.");
        var text_conditions = new HashSet<string> { "mode", "visibility", "bounds", "custom_data", "rhi" };
        foreach (var key in RequiredConditions.Where(key => !text_conditions.Contains(key)))
            if (!double.TryParse(conditions[key], NumberStyles.Float, CultureInfo.InvariantCulture, out var value) || !double.IsFinite(value))
                throw new BenchmarkToolException($"Invalid numeric comparability condition: {key}");
        foreach (var (key, expected) in request.Conditions())
        {
            var actual = conditions[key];
            var equal = double.TryParse(expected, NumberStyles.Float, CultureInfo.InvariantCulture, out var number)
                ? double.TryParse(actual, NumberStyles.Float, CultureInfo.InvariantCulture, out var observed) && double.IsFinite(observed) && Math.Abs(number - observed) <= Math.Max(1e-6, Math.Abs(number) * 1e-6)
                : actual == expected;
            if (!equal) throw new BenchmarkToolException($"SandboxISMC condition mismatch: {key} requested {expected}, observed {actual}.");
        }
        if (!result.GetProperty("complete").GetBoolean()) throw new BenchmarkToolException("SandboxISMC did not complete measurement (viewport failure or early termination); see unreal.log.");
        var metrics = ReadCsv(run.Artifact("metrics.csv"), conditions);
        run.Manifest.Comparability = conditions;
        run.Publish();
        return new SandboxIsmcCapture(run.Manifest.RunId, run.DirectoryPath, repetition, conditions, metrics);
    }

    internal static IReadOnlyList<BenchmarkMetric> ReadCsv(string path, IReadOnlyDictionary<string, string> conditions)
    {
        using var reader = new StreamReader(path);
        var header = BenchmarkCommandSupport.ParseCsv(reader.ReadLine() ?? throw new BenchmarkToolException("Empty SandboxISMC CSV."));
        if (header.Count != header.Distinct(StringComparer.Ordinal).Count()) throw new BenchmarkToolException("Duplicate SandboxISMC CSV columns.");
        var columns = header.Select((name, index) => (name, index)).ToDictionary(item => item.name, item => item.index, StringComparer.Ordinal);
        var summaries = new HashSet<string> { "samples", "min", "median", "p95", "max" };
        var required = new[] { "renderer", "metric", "unit", "mode", "instances", "visibility", "bounds", "custom_data", "update_percent", "churn", "min_instances",
            "half_cycle_updates", "replacement_percent", "warmup_updates", "warmup_seconds", "measurement_seconds", "updated_instances" }.Concat(summaries);
        foreach (var name in required)
            if (!columns.ContainsKey(name)) throw new BenchmarkToolException($"SandboxISMC CSV lacks {name}.");
        var metrics = new List<BenchmarkMetric>();
        while (reader.ReadLine() is { } line)
        {
            if (string.IsNullOrWhiteSpace(line)) continue;
            var row = BenchmarkCommandSupport.ParseCsv(line);
            if (row.Count != header.Count) throw new BenchmarkToolException("SandboxISMC CSV row has incorrect column count.");
            string Value(string name) => row[columns[name]];
            double Number(string name) => double.TryParse(Value(name), NumberStyles.Float, CultureInfo.InvariantCulture, out var value) && double.IsFinite(value)
                ? value : throw new BenchmarkToolException($"Invalid CSV numeric value: {name}");
            foreach (var key in conditions.Keys.Where(columns.ContainsKey))
            {
                // Existing CSV writes three decimal places for workload percentages and durations.
                var expected = conditions[key];
                var actual = Value(key);
                var equal = double.TryParse(expected, NumberStyles.Float, CultureInfo.InvariantCulture, out var numeric)
                    ? Math.Abs(Number(key) - numeric) <= .000501 : expected == actual;
                if (!equal) throw new BenchmarkToolException($"CSV/conditions mismatch: {key}");
            }
            if (!int.TryParse(Value("samples"), NumberStyles.None, CultureInfo.InvariantCulture, out var samples)) throw new BenchmarkToolException("Invalid sample count.");
            var dimensions = columns.Keys.Where(key => key is not "metric" and not "unit" && !summaries.Contains(key)).ToDictionary(key => key, Value, StringComparer.Ordinal);
            metrics.Add(new BenchmarkMetric(new MetricIdentity(Value("metric"), Value("unit"), dimensions),
                new MetricSummary(samples, Number("min"), Number("median"), Number("p95"), Number("max"))));
        }
        _ = BenchmarkMetrics.Index(metrics);
        return metrics;
    }

    public static BenchmarkComparison Compare(IReadOnlyList<SandboxIsmcCapture> captures)
    {
        var measured = captures.Where(item => !item.Repetition.Warmup).ToArray();
        if (measured.Length == 0) throw new BenchmarkToolException("No complete measured repetitions.");
        var reference = measured[0];
        foreach (var capture in measured.Skip(1))
        {
            var validation = BenchmarkMetrics.Compare(reference.Metrics, capture.Metrics, reference.Conditions, capture.Conditions);
            if (!validation.Comparable) return validation;
        }
        IReadOnlyList<BenchmarkMetric> Aggregate(string side)
        {
            var runs = measured.Where(item => item.Repetition.Side == side).ToArray();
            if (runs.Length == 0) throw new BenchmarkToolException($"No {side} repetitions.");
            var indexes = runs.Select(run => BenchmarkMetrics.Index(run.Metrics)).ToArray();
            return runs[0].Metrics.Select(metric => new BenchmarkMetric(metric.Identity,
                MetricSummary.AcrossRuns(indexes.Select(index => index[metric.Identity.Key].Summary.Median)))).ToArray();
        }
        return BenchmarkMetrics.Compare(Aggregate("baseline"), Aggregate("candidate"), reference.Conditions, reference.Conditions);
    }

    public static void WriteReport(string directory, BenchmarkComparison comparison)
    {
        BenchmarkCommandSupport.WriteJson(Path.Combine(directory, "comparison.json"), comparison);
        var report = new StringBuilder("# SandboxISMC revision comparison\n\n");
        if (!comparison.Comparable)
        {
            report.AppendLine("Incomparable. No performance deltas were calculated.");
            foreach (var error in comparison.Errors) report.AppendLine($"- {error}");
        }
        else
        {
            report.AppendLine("Values compare distributions of complete-run medians. Samples in this table count independent repetitions; captures.json preserves within-run frame summaries. CPU upload, thread and GPU timings retain their original identities.\n");
            report.AppendLine("| Renderer | Metric | Unit | Runs A/B | Baseline median | Candidate median | Delta | Delta % |\n|---|---|---|---:|---:|---:|---:|---:|");
            foreach (var item in comparison.Metrics)
                report.AppendLine(FormattableString.Invariant($"| {item.Identity.Dimensions.GetValueOrDefault("renderer")} | {item.Identity.Metric} | {item.Identity.Unit} | {item.Baseline.Samples}/{item.Candidate.Samples} | {item.Baseline.Median:G6} | {item.Candidate.Median:G6} | {item.Delta:G6} | {item.DeltaPercent:G6} |"));
        }
        File.WriteAllText(Path.Combine(directory, "comparison.md"), report.ToString());
        BenchmarkCommandSupport.WriteCsv(Path.Combine(directory, "comparison.csv"), new[] { new[] { "renderer", "metric", "unit", "baseline_runs", "candidate_runs", "baseline_median", "candidate_median", "delta", "delta_percent" } }
            .Concat(comparison.Metrics.Select(item => new[] { item.Identity.Dimensions.GetValueOrDefault("renderer", ""), item.Identity.Metric, item.Identity.Unit,
                item.Baseline.Samples.ToString(CultureInfo.InvariantCulture), item.Candidate.Samples.ToString(CultureInfo.InvariantCulture),
                item.Baseline.Median.ToString("R", CultureInfo.InvariantCulture), item.Candidate.Median.ToString("R", CultureInfo.InvariantCulture),
                item.Delta.ToString("R", CultureInfo.InvariantCulture), item.DeltaPercent?.ToString("R", CultureInfo.InvariantCulture) ?? "" })));
    }
}
