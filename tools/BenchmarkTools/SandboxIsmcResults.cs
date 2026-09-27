using System.Globalization;
using System.Text;
using System.Text.Json;

namespace BenchmarkTools;

internal sealed record SandboxIsmcCapture(string RunId, string Directory, BenchmarkRepetition Repetition,
    IReadOnlyDictionary<string, string> Conditions, IReadOnlyList<BenchmarkMetric> Metrics, int SchemaVersion = 1);
internal sealed record SandboxIsmcReportMetadata(string RunId, string? Label, bool ValidationOnly,
    RevisionIdentity Baseline, RevisionIdentity Candidate, int Repetitions, IReadOnlyList<BenchmarkRepetition> Sequence,
    IReadOnlyDictionary<string, string> Conditions);

internal static class SandboxIsmcResults
{
    internal static readonly string[] RequiredConditions = ["result_schema", "mode", "visibility", "bounds", "custom_data", "rhi", "instances", "update_percent",
        "churn", "min_instances", "half_cycle_updates", "replacement_percent", "warmup_updates", "warmup_seconds", "measurement_seconds", "shadows", "trace",
        "requested_width", "requested_height", "observed_width", "observed_height", "grid_spacing", "grid_gap", "movement_amplitude", "movement_frequency", "rotation_speed",
        "frame_limits_disabled", "r.ScreenPercentage", "r.DynamicRes.OperationMode", "r.VSync", "r.VSyncEditor", "t.MaxFPS"];

    public static IReadOnlyDictionary<string, string> ReadTerminal(BenchmarkRunContext run, SandboxIsmcRequest request)
    {
        try { return ReadTerminalResult(run, request); }
        catch (Exception error) when (error is KeyNotFoundException or InvalidOperationException or FormatException or ArgumentException)
        {
            throw new BenchmarkToolException($"Malformed SandboxISMC result: {error.Message}");
        }
    }

    private static IReadOnlyDictionary<string, string> ReadTerminalResult(BenchmarkRunContext run, SandboxIsmcRequest request)
    {
        using var json = JsonDocument.Parse(File.ReadAllText(run.Artifact("result.json")));
        var result = json.RootElement;
        if (result.GetProperty("schemaVersion").GetInt32() != 1 || result.GetProperty("runId").GetString() != run.Manifest.RunId)
            throw new BenchmarkToolException("SandboxISMC result schema/run identity mismatch.");
        var conditions = result.GetProperty("conditions").EnumerateObject().ToDictionary(item => item.Name, item => item.Value.GetString()!, StringComparer.Ordinal);
        run.Manifest.Comparability = conditions;
        run.Publish();
        if (!result.GetProperty("complete").GetBoolean())
        {
            var error = result.TryGetProperty("error", out var detail) ? detail.GetString() : null;
            throw new BenchmarkToolException($"SandboxISMC did not complete: {error ?? "viewport failure or early termination; see unreal.log"}");
        }
        ValidateConditions(conditions);
        ValidateRequest(conditions, request);
        return conditions;
    }

    internal static void ValidateConditions(IReadOnlyDictionary<string, string> conditions)
    {
        foreach (var key in RequiredConditions)
            if (!conditions.TryGetValue(key, out var value) || string.IsNullOrWhiteSpace(value)) throw new BenchmarkToolException($"Missing comparability condition: {key}");
        if (conditions["result_schema"] != "1") throw new BenchmarkToolException("Unsupported SandboxISMC conditions schema.");
        var text_conditions = new HashSet<string> { "mode", "visibility", "bounds", "custom_data", "rhi" };
        foreach (var key in RequiredConditions.Where(key => !text_conditions.Contains(key)))
            if (!double.TryParse(conditions[key], NumberStyles.Float, CultureInfo.InvariantCulture, out var value) || !double.IsFinite(value))
                throw new BenchmarkToolException($"Invalid numeric comparability condition: {key}");
    }

    internal static void ValidateRequest(IReadOnlyDictionary<string, string> conditions, SandboxIsmcRequest request)
    {
        foreach (var (key, expected) in request.Conditions())
        {
            var actual = conditions[key];
            var equal = double.TryParse(expected, NumberStyles.Float, CultureInfo.InvariantCulture, out var number)
                ? double.TryParse(actual, NumberStyles.Float, CultureInfo.InvariantCulture, out var observed) && double.IsFinite(observed) && Math.Abs(number - observed) <= Math.Max(1e-6, Math.Abs(number) * 1e-6)
                : actual == expected;
            if (!equal) throw new BenchmarkToolException($"SandboxISMC condition mismatch: {key} requested {expected}, observed {actual}.");
        }
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

    public static PairedBenchmarkComparison Compare(IReadOnlyList<SandboxIsmcCapture> captures)
    {
        var measured = captures.Where(item => !item.Repetition.Warmup).ToArray();
        if (measured.Length == 0) throw new BenchmarkToolException("No complete measured repetitions.");
        var pairs = measured.GroupBy(item => item.Repetition.Repetition).OrderBy(pair => pair.Key).ToArray();
        foreach (var pair in pairs)
            if (pair.Key <= 0 || pair.Count() != 2 || pair.Count(item => item.Repetition.Side == "baseline") != 1 || pair.Count(item => item.Repetition.Side == "candidate") != 1)
                throw new BenchmarkToolException($"Repetition {pair.Key} requires exactly one measured baseline and one candidate; missing or duplicate side.");
        var reference = measured[0];
        foreach (var capture in measured.Skip(1))
        {
            var validation = BenchmarkMetrics.Compare(reference.Metrics, capture.Metrics, reference.Conditions, capture.Conditions);
            if (!validation.Comparable) return new PairedBenchmarkComparison(false, validation.Errors, []);
        }
        var deltas = new Dictionary<string, List<PairedMetricDelta>>(StringComparer.Ordinal);
        foreach (var pair in pairs)
        {
            var baseline = pair.Single(item => item.Repetition.Side == "baseline");
            var candidate = pair.Single(item => item.Repetition.Side == "candidate");
            var comparison = BenchmarkMetrics.Compare(baseline.Metrics, candidate.Metrics, baseline.Conditions, candidate.Conditions);
            foreach (var metric in comparison.Metrics)
            {
                if (!deltas.TryGetValue(metric.Identity.Key, out var values)) deltas[metric.Identity.Key] = values = [];
                values.Add(new PairedMetricDelta(pair.Key, metric.Baseline.Median, metric.Candidate.Median, metric.Delta, metric.DeltaPercent));
            }
        }
        return new PairedBenchmarkComparison(true, [], reference.Metrics.OrderBy(metric => metric.Identity.Key, StringComparer.Ordinal).Select(metric =>
        {
            var values = deltas[metric.Identity.Key];
            return new PairedMetricSummary(metric.Identity, MetricSummary.AcrossRuns(values.Select(value => value.Baseline)),
                MetricSummary.AcrossRuns(values.Select(value => value.Candidate)), MetricSummary.AcrossRuns(values.Select(value => value.Delta)),
                values.All(value => value.DeltaPercent.HasValue) ? MetricSummary.AcrossRuns(values.Select(value => value.DeltaPercent!.Value)) : null, values);
        }).ToArray());
    }

    public static PairedBenchmarkComparison GenerateReports(string directory, BenchmarkManifest manifest, BenchmarkMeasurementPlan plan,
        IReadOnlyList<SandboxIsmcCapture> captures)
    {
        if (plan.Ismc is null) throw new BenchmarkToolException("Comparison plan has no SandboxISMC workload.");
        if (captures.Any(item => item is null) || plan.Sequence.Any(item => item is null))
            throw new BenchmarkToolException("Null capture or sequence entry.");
        if (captures.Count != plan.Sequence.Count ||
            !captures.Select(item => item.Repetition).OrderBy(item => item.Sequence).SequenceEqual(plan.Sequence.OrderBy(item => item.Sequence)))
            throw new BenchmarkToolException("Incomplete comparison: captures do not match the planned sequence.");
        if (captures.Select(item => item.RunId).Distinct(StringComparer.Ordinal).Count() != captures.Count ||
            captures.Select(item => item.Repetition.Sequence).Distinct().Count() != captures.Count)
            throw new BenchmarkToolException("Duplicate capture run or sequence identity.");
        foreach (var capture in captures)
        {
            if (capture.SchemaVersion != 1 || string.IsNullOrWhiteSpace(capture.RunId) || capture.Repetition.Sequence <= 0 ||
                capture.Repetition.Repetition <= 0 || capture.Repetition.Side is not ("baseline" or "candidate"))
                throw new BenchmarkToolException("Unsupported capture schema or invalid capture identity.");
            ValidateConditions(capture.Conditions);
            _ = BenchmarkMetrics.Index(capture.Metrics);
        }
        var comparison = Compare(captures);
        if (comparison.Comparable)
            foreach (var capture in captures.Where(item => !item.Repetition.Warmup)) ValidateRequest(capture.Conditions, plan.Ismc);
        var metadata = new SandboxIsmcReportMetadata(manifest.RunId, manifest.Label, plan.ValidationOnly,
            plan.Baseline, plan.Candidate, captures.Where(item => !item.Repetition.Warmup).Select(item => item.Repetition.Repetition).Distinct().Count(),
            plan.Sequence.OrderBy(item => item.Sequence).ToArray(), captures.First(item => !item.Repetition.Warmup).Conditions);
        WriteReport(directory, comparison, metadata);
        return comparison;
    }

    private static void WriteReport(string directory, PairedBenchmarkComparison comparison, SandboxIsmcReportMetadata metadata)
    {
        BenchmarkCommandSupport.WriteJson(Path.Combine(directory, "comparison.json"), new { SchemaVersion = 2, Metadata = metadata,
            comparison.Comparable, comparison.Errors, comparison.Metrics });
        var report = new StringBuilder("# SandboxISMC revision comparison\n\n");
        if (!string.IsNullOrWhiteSpace(metadata.Label)) report.AppendLine($"{Text(metadata.Label)}\n");
        if (metadata.ValidationOnly) report.AppendLine("**Validation only — protocol/comparability smoke; no performance conclusions.**\n");
        report.AppendLine($"Run: {Text(metadata.RunId)}. Baseline: {Revision(metadata.Baseline)}. Candidate: {Revision(metadata.Candidate)}.");
        report.AppendLine($"Measured repetitions per side: {metadata.Repetitions}. Order: {string.Join(", ", metadata.Sequence.Select(item => $"{(item.Side == "baseline" ? "A" : "B")}{item.Repetition}{(item.Warmup ? " (warmup)" : "")}"))}.\n");
        var c = metadata.Conditions;
        string Condition(string key) => Text(c.GetValueOrDefault(key, "unknown"));
        report.AppendLine($"Viewport requested/observed: {Condition("requested_width")}×{Condition("requested_height")} / {Condition("observed_width")}×{Condition("observed_height")}; RHI: {Condition("rhi")}.");
        report.AppendLine($"Mode: {Condition("mode")}; instances: {Condition("instances")}; update: {Condition("update_percent")}%.");
        report.AppendLine($"Bounds: {Condition("bounds")}; visibility: {Condition("visibility")}; custom data: {Condition("custom_data")}; shadows: {Condition("shadows")}; trace: {Condition("trace")}.");
        report.AppendLine($"Churn: {Condition("churn")}; minimum: {Condition("min_instances")}; half-cycle: {Condition("half_cycle_updates")} updates; replacement: {Condition("replacement_percent")}%.");
        report.AppendLine($"Warmup: {Condition("warmup_seconds")} s / {Condition("warmup_updates")} updates; measurement: {Condition("measurement_seconds")} s per process.");
        report.AppendLine($"Frame limits disabled: {Condition("frame_limits_disabled")}; VSync/Editor: {Condition("r.VSync")}/{Condition("r.VSyncEditor")}; MaxFPS: {Condition("t.MaxFPS")}; screen percentage: {Condition("r.ScreenPercentage")}; dynamic resolution: {Condition("r.DynamicRes.OperationMode")}.\n");
        if (!comparison.Comparable)
        {
            report.AppendLine("Incomparable. No performance deltas were calculated.");
            foreach (var error in comparison.Errors) report.AppendLine($"- {error}");
        }
        else
        {
            report.AppendLine("Baseline/candidate values summarize complete-run medians. Deltas summarize paired candidate-minus-baseline complete-run differences by repetition ID. Samples count independent complete repetitions; captures.json preserves within-run frame summaries. Medians average the two middle values for even sample counts; p95 uses nearest rank. Percent summaries are unavailable if any baseline is zero.\n");
            report.AppendLine("| Renderer | Metric | Unit | Pairs | Baseline median | Candidate median | Paired delta median | Paired delta % median |\n|---|---|---|---:|---:|---:|---:|---:|");
            foreach (var item in comparison.Metrics)
                report.AppendLine(FormattableString.Invariant($"| {item.Identity.Dimensions.GetValueOrDefault("renderer")} | {item.Identity.Metric} | {item.Identity.Unit} | {item.Delta.Samples} | {item.Baseline.Median:G6} | {item.Candidate.Median:G6} | {item.Delta.Median:G6} | {item.DeltaPercent?.Median:G6} |"));
        }
        BenchmarkCommandSupport.WriteText(Path.Combine(directory, "comparison.md"), report.ToString());
        BenchmarkCommandSupport.WriteCsv(Path.Combine(directory, "comparison.csv"), new[] { new[] { "renderer", "metric", "unit", "baseline_runs", "candidate_runs", "baseline_run_median", "candidate_run_median", "paired_delta_median", "paired_delta_percent_median" } }
            .Concat(comparison.Metrics.Select(item => new[] { item.Identity.Dimensions.GetValueOrDefault("renderer", ""), item.Identity.Metric, item.Identity.Unit,
                item.Baseline.Samples.ToString(CultureInfo.InvariantCulture), item.Candidate.Samples.ToString(CultureInfo.InvariantCulture),
                item.Baseline.Median.ToString("R", CultureInfo.InvariantCulture), item.Candidate.Median.ToString("R", CultureInfo.InvariantCulture),
                item.Delta.Median.ToString("R", CultureInfo.InvariantCulture), item.DeltaPercent?.Median.ToString("R", CultureInfo.InvariantCulture) ?? "" })));
    }

    private static string Revision(RevisionIdentity identity) => Text(identity.Commit[..Math.Min(12, identity.Commit.Length)]) + (identity.Dirty ? " (dirty)" : " (clean)");
    private static string Text(string value) => value.Replace("&", "&amp;", StringComparison.Ordinal).Replace("<", "&lt;", StringComparison.Ordinal)
        .Replace(">", "&gt;", StringComparison.Ordinal).Replace("|", "\\|", StringComparison.Ordinal).Replace("*", "\\*", StringComparison.Ordinal)
        .Replace("\r", " ", StringComparison.Ordinal).Replace("\n", " ", StringComparison.Ordinal);
}
