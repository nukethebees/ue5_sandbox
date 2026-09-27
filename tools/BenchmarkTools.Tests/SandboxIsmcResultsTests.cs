using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace BenchmarkTools.Tests;

[TestClass]
public sealed class SandboxIsmcResultsTests
{
    [TestMethod]
    public void Pairing_uses_repetition_identity_and_summarizes_paired_deltas()
    {
        SandboxIsmcCapture[] captures = [Capture(1, "baseline", 1), Capture(1, "candidate", 10),
            Capture(2, "candidate", 20), Capture(2, "baseline", 100), Capture(3, "baseline", 101), Capture(3, "candidate", 110)];
        var metric = SandboxIsmcResults.Compare(captures).Metrics.Single();
        Assert.AreEqual(100, metric.Baseline.Median);
        Assert.AreEqual(20, metric.Candidate.Median);
        Assert.AreEqual(9, metric.Delta.Median); // Independent medians would produce -80.
        Assert.AreEqual(3, metric.Delta.Samples);
        CollectionAssert.AreEqual(new[] { 9d, -80, 9 }, metric.Pairs.Select(pair => pair.Delta).ToArray());
        var reordered = SandboxIsmcResults.Compare(captures.Reverse().ToArray()).Metrics.Single();
        Assert.AreEqual(metric.Delta, reordered.Delta);
        CollectionAssert.AreEqual(metric.Pairs.ToArray(), reordered.Pairs.ToArray());
        var two = SandboxIsmcResults.Compare(captures.Take(4).ToArray()).Metrics.Single();
        Assert.AreEqual(50.5, two.Baseline.Median);
        Assert.AreEqual(15, two.Candidate.Median);
        Assert.AreEqual(-35.5, two.Delta.Median);
        Assert.AreEqual(9, two.Delta.P95);
    }

    [TestMethod]
    public void Missing_or_duplicate_repetition_sides_fail()
    {
        Assert.ThrowsException<BenchmarkToolException>(() => SandboxIsmcResults.Compare([Capture(1, "baseline", 1)]));
        Assert.ThrowsException<BenchmarkToolException>(() => SandboxIsmcResults.Compare([Capture(1, "candidate", 1)]));
        Assert.ThrowsException<BenchmarkToolException>(() => SandboxIsmcResults.Compare([Capture(1, "baseline", 1), Capture(1, "baseline", 2)]));
        Assert.ThrowsException<BenchmarkToolException>(() => SandboxIsmcResults.Compare([Capture(1, "baseline", 1), Capture(1, "candidate", 2), Capture(1, "candidate", 3)]));
    }

    internal static SandboxIsmcCapture Capture(int repetition, string side, double value) => new(
        $"{side}-{repetition}", "unused", new BenchmarkRepetition(repetition * 2 + (side == "baseline" ? 0 : 1), repetition, side, false),
        new Dictionary<string, string> { ["result_schema"] = "1" }, [BenchmarkInfrastructureTests.Metric(value)]);
}
