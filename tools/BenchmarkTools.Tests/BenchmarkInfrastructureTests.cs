using System.Text.Json;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace BenchmarkTools.Tests;

[TestClass]
public sealed class BenchmarkInfrastructureTests
{
    [TestMethod]
    public void Owned_runs_are_unique_and_publish_success_and_failure_manifests()
    {
        using var temporary = new BenchmarkTestDirectory();
        var first = BenchmarkRunContext.Create(new RepositoryPaths(temporary.Root), "test");
        var second = BenchmarkRunContext.Create(new RepositoryPaths(temporary.Root), "test");
        Assert.AreNotEqual(first.DirectoryPath, second.DirectoryPath);
        first.Expect("metrics.csv");
        Assert.ThrowsException<BenchmarkToolException>(first.ValidateArtifacts);
        File.WriteAllText(first.Artifact("metrics.csv"), "result");
        first.ValidateArtifacts();
        first.Complete();
        second.Fail(new IOException("launch failed"));
        using var success = JsonDocument.Parse(File.ReadAllText(first.Artifact("manifest.json")));
        using var failure = JsonDocument.Parse(File.ReadAllText(second.Artifact("manifest.json")));
        Assert.AreEqual(1, success.RootElement.GetProperty("schemaVersion").GetInt32());
        Assert.AreEqual("complete", success.RootElement.GetProperty("status").GetString());
        Assert.AreEqual(first.Artifact("metrics.csv"), success.RootElement.GetProperty("artifacts").GetProperty("metrics.csv").GetString());
        Assert.AreEqual("failed", failure.RootElement.GetProperty("status").GetString());
        Assert.AreEqual("launch failed", failure.RootElement.GetProperty("failure").GetString());
        Assert.AreEqual(0, Directory.GetFiles(temporary.Root, "*.tmp", SearchOption.AllDirectories).Length);
    }

    [TestMethod]
    public void Ordering_is_balanced_with_separate_complete_warmup_runs()
    {
        var sequence = BenchmarkOrdering.Balanced(4, 1);
        CollectionAssert.AreEqual(new[] { "baseline", "candidate", "candidate", "baseline", "baseline", "candidate", "candidate", "baseline" },
            sequence.Where(item => !item.Warmup).Select(item => item.Side).ToArray());
        CollectionAssert.AreEqual(Enumerable.Range(1, 10).ToArray(), sequence.Select(item => item.Sequence).ToArray());
        Assert.IsTrue(sequence.Take(2).All(item => item.Warmup));
        foreach (var pair in sequence.Where(item => !item.Warmup).GroupBy(item => item.Repetition)) Assert.AreEqual(2, pair.Count());
    }

    [TestMethod]
    public void Comparison_pairs_metrics_and_preserves_run_level_distributions()
    {
        var result = BenchmarkMetrics.Compare([Metric(10)], [Metric(12)], Conditions(), Conditions());
        Assert.IsTrue(result.Comparable);
        Assert.AreEqual(2, result.Metrics.Single().Delta);
        Assert.AreEqual(20, result.Metrics.Single().DeltaPercent);
        var summary = MetricSummary.AcrossRuns([1, 4, 2, 3]);
        Assert.AreEqual(4, summary.Samples);
        Assert.AreEqual(2, summary.Median);
        Assert.AreEqual(4, summary.P95);
    }

    [TestMethod]
    public void Comparison_rejects_duplicate_missing_units_dimensions_and_conditions()
    {
        Assert.ThrowsException<BenchmarkToolException>(() => BenchmarkMetrics.Index([Metric(1), Metric(2)]));
        Assert.ThrowsException<BenchmarkToolException>(() => BenchmarkMetrics.Index([Metric(double.NaN)]));
        Assert.ThrowsException<BenchmarkToolException>(() => BenchmarkMetrics.Index([Metric(double.PositiveInfinity)]));
        AssertInvalid([Metric(1), Metric(2, metric: "gpu")], [Metric(1)], "Missing metric");
        AssertInvalid([Metric(1)], [Metric(1, unit: "bytes")], "Unit mismatch");
        AssertInvalid([Metric(1)], [Metric(1, renderer: "engine")], "dimension mismatch");
        var result = BenchmarkMetrics.Compare([Metric(1)], [Metric(2)], Conditions(), new Dictionary<string, string> { ["viewport"] = "1920x1080" });
        Assert.IsFalse(result.Comparable);
        Assert.AreEqual(0, result.Metrics.Count);
    }

    [TestMethod]
    public async Task Revision_session_resolves_detached_commit_and_preserves_candidate_and_supplied_worktree()
    {
        using var directory = new BenchmarkTestDirectory();
        var runner = new ProcessRunner();
        var app = new BenchmarkToolsApplication(runner, new TestJobserver(), new TestEnvironment(), TextWriter.Null, TextWriter.Null, "unused");
        await Git("init", "--initial-branch=candidate");
        await Git("config", "user.name", "Benchmark test");
        await Git("config", "user.email", "benchmark@example.invalid");
        File.WriteAllText(Path.Combine(directory.Root, "tracked.txt"), "original");
        File.WriteAllText(Path.Combine(directory.Root, ".gitignore"), ".local/\n");
        await Git("add", ".");
        await Git("commit", "-m", "fixture");
        var head = await Git("rev-parse", "HEAD");
        File.WriteAllText(Path.Combine(directory.Root, "tracked.txt"), "candidate edit");
        var before = await Git("status", "--porcelain");
        string owned;
        await using (var session = await RevisionComparisonSession.CreateAsync(app, new RepositoryPaths(directory.Root), "HEAD", "test-owned", null, false, default))
        {
            owned = session.BaselineRoot;
            Assert.IsTrue(session.Candidate.Dirty);
            Assert.AreEqual(head, session.Baseline.Commit);
            var detached = await runner.RunAsync(BenchmarkGit.Request(owned, ["symbolic-ref", "-q", "HEAD"]), default);
            Assert.AreNotEqual(0, detached.ExitCode);
            await using var supplied = await RevisionComparisonSession.CreateAsync(app, new RepositoryPaths(directory.Root), "HEAD", "supplied", owned, false, default);
            Assert.IsFalse(supplied.OwnsBaseline);
        }
        Assert.IsFalse(Directory.Exists(owned));
        Assert.AreEqual(before, await Git("status", "--porcelain"));
        Assert.AreEqual("candidate", await Git("branch", "--show-current"));
        Assert.AreEqual("candidate edit", File.ReadAllText(Path.Combine(directory.Root, "tracked.txt")));

        async Task<string> Git(params string[] args) => await BenchmarkGit.TextAsync(app, directory.Root, args, default);
    }

    [TestMethod]
    public void Worktree_paths_reject_escape_and_parent_itself()
    {
        using var directory = new BenchmarkTestDirectory();
        var parent = Path.Combine(directory.Root, ".local", "benchmarks", "worktrees");
        RevisionComparisonSession.ValidateOwnedPath(Path.Combine(parent, "run", "baseline"), parent);
        Assert.ThrowsException<BenchmarkToolException>(() => RevisionComparisonSession.ValidateOwnedPath(parent, parent));
        Assert.ThrowsException<BenchmarkToolException>(() => RevisionComparisonSession.ValidateOwnedPath(Path.Combine(parent, "..", "escape"), parent));
        Assert.ThrowsException<BenchmarkToolException>(() => RevisionComparisonSession.ValidateOwnedPath(Path.Combine(directory.Root, ".local", "worktrees", "branch"), parent));
    }

    internal static BenchmarkMetric Metric(double value, string unit = "ms", string renderer = "custom", string metric = "frame") =>
        new(new MetricIdentity(metric, unit, new Dictionary<string, string> { ["renderer"] = renderer }), new MetricSummary(5, value, value, value, value));
    private static Dictionary<string, string> Conditions() => new() { ["viewport"] = "1280x720" };
    private static void AssertInvalid(BenchmarkMetric[] a, BenchmarkMetric[] b, string error)
    {
        var result = BenchmarkMetrics.Compare(a, b, Conditions(), Conditions());
        Assert.IsFalse(result.Comparable);
        Assert.AreEqual(0, result.Metrics.Count);
        StringAssert.Contains(string.Join(";", result.Errors), error);
    }
}

internal sealed class BenchmarkTestDirectory : IDisposable
{
    public string Root { get; } = Path.Combine(Path.GetTempPath(), "BenchmarkToolsOwned-" + Guid.NewGuid().ToString("N"));
    public BenchmarkTestDirectory()
    {
        Directory.CreateDirectory(Root);
    }
    public void Dispose()
    {
        var full = Path.GetFullPath(Root);
        if (Path.GetDirectoryName(full) != Path.GetFullPath(Path.GetTempPath()).TrimEnd(Path.DirectorySeparatorChar) || !Path.GetFileName(full).StartsWith("BenchmarkToolsOwned-", StringComparison.Ordinal))
            throw new InvalidOperationException("Refusing unexpected test cleanup path.");
        foreach (var file in Directory.EnumerateFiles(full, "*", SearchOption.AllDirectories)) File.SetAttributes(file, FileAttributes.Normal);
        Directory.Delete(full, true);
    }
}

internal sealed class TestJobserver : IJobserverLocator { public string Locate() => "test-jobserver"; }
internal sealed class TestEnvironment(string? job = null) : IEnvironment { public string? GetEnvironmentVariable(string name) => name == "NUKETHEBEES_JOBSERVER_JOB" ? job : null; }
