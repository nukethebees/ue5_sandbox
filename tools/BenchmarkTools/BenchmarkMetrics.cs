using System.Text.Json;

namespace BenchmarkTools;

internal sealed record MetricIdentity(string Metric, string Unit, IReadOnlyDictionary<string, string> Dimensions)
{
    public string Key => JsonSerializer.Serialize(new { Metric, Dimensions = Dimensions.OrderBy(item => item.Key, StringComparer.Ordinal) });
}

internal sealed record MetricSummary(int Samples, double Min, double Median, double P95, double Max)
{
    public void Validate()
    {
        if (Samples <= 0 || !new[] { Min, Median, P95, Max }.All(double.IsFinite) || Min > Median || Median > P95 || P95 > Max)
            throw new BenchmarkToolException("Metric summary has no samples, non-finite values, or inconsistent quantiles.");
    }

    public static MetricSummary AcrossRuns(IEnumerable<double> values)
    {
        var sorted = values.Order().ToArray();
        if (sorted.Length == 0 || !sorted.All(double.IsFinite)) throw new BenchmarkToolException("Run summaries must contain finite values.");
        return new MetricSummary(sorted.Length, sorted[0], sorted[(sorted.Length - 1) / 2], sorted[(int)Math.Ceiling(.95 * sorted.Length) - 1], sorted[^1]);
    }
}

internal sealed record BenchmarkMetric(MetricIdentity Identity, MetricSummary Summary);
internal sealed record MetricDelta(MetricIdentity Identity, MetricSummary Baseline, MetricSummary Candidate, double Delta, double? DeltaPercent);
internal sealed record BenchmarkComparison(bool Comparable, IReadOnlyList<string> Errors, IReadOnlyList<MetricDelta> Metrics);

internal static class BenchmarkMetrics
{
    public static Dictionary<string, BenchmarkMetric> Index(IEnumerable<BenchmarkMetric> metrics)
    {
        var result = new Dictionary<string, BenchmarkMetric>(StringComparer.Ordinal);
        foreach (var metric in metrics)
        {
            metric.Summary.Validate();
            if (string.IsNullOrWhiteSpace(metric.Identity.Metric) || string.IsNullOrWhiteSpace(metric.Identity.Unit))
                throw new BenchmarkToolException("Metric name and unit are required.");
            if (!result.TryAdd(metric.Identity.Key, metric)) throw new BenchmarkToolException($"Duplicate metric identity: {metric.Identity.Key}");
        }
        if (result.Count == 0) throw new BenchmarkToolException("Benchmark contains no metrics.");
        return result;
    }

    public static BenchmarkComparison Compare(IReadOnlyList<BenchmarkMetric> baseline, IReadOnlyList<BenchmarkMetric> candidate,
        IReadOnlyDictionary<string, string> baseline_configuration, IReadOnlyDictionary<string, string> candidate_configuration)
    {
        var left = Index(baseline);
        var right = Index(candidate);
        var errors = new List<string>();
        foreach (var key in baseline_configuration.Keys.Union(candidate_configuration.Keys).Order(StringComparer.Ordinal))
            if (!baseline_configuration.TryGetValue(key, out var a) || !candidate_configuration.TryGetValue(key, out var b) || a != b)
                errors.Add($"Comparability mismatch: {key}");
        foreach (var key in left.Keys.Union(right.Keys).Order(StringComparer.Ordinal))
        {
            if (!left.TryGetValue(key, out var a) || !right.TryGetValue(key, out var b)) errors.Add($"Missing metric or dimension mismatch: {key}");
            else if (a.Identity.Unit != b.Identity.Unit) errors.Add($"Unit mismatch: {key}");
        }
        if (errors.Count != 0) return new BenchmarkComparison(false, errors, []);
        return new BenchmarkComparison(true, [], left.Select(item =>
        {
            var a = item.Value;
            var b = right[item.Key];
            var delta = b.Summary.Median - a.Summary.Median;
            var percent = a.Summary.Median == 0 ? (double?)null : 100 * delta / a.Summary.Median;
            if (!double.IsFinite(delta) || percent is { } p && !double.IsFinite(p))
                throw new BenchmarkToolException("Metric delta overflowed.");
            return new MetricDelta(a.Identity, a.Summary, b.Summary, delta, percent);
        }).ToArray());
    }
}
