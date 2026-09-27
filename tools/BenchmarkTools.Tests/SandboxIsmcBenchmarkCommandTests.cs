using System.Globalization;
using System.Text.Json;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace BenchmarkTools.Tests;

[TestClass]
public sealed class SandboxIsmcBenchmarkCommandTests
{
    [TestMethod]
    public async Task Comparison_builds_before_one_lease_and_interleaves_complete_runs()
    {
        using var fixture = new Fixture();
        var result = await fixture.Run("sandbox-ismc-revision-ab", "--baseline", "old", "--repetitions", "2", "--warmup-runs", "1");
        Assert.AreEqual(0, result, fixture.Errors.ToString());
        var lease = fixture.Runner.Requests.Single(item => item.FileName == "test-jobserver");
        CollectionAssert.IsSubsetOf(new[] { "--exclusive", "machine", "benchmark", "--shared" }, lease.Arguments.ToArray());
        Assert.AreEqual(2, lease.Arguments.Count(item => item == "--exclusive"));
        Assert.AreEqual(4, fixture.Runner.Requests.Count(item => item.FileName == "cmake"));
        Assert.IsTrue(fixture.Runner.Requests.TakeWhile(item => item != lease).Count(item => item.FileName == "cmake") == 4);
        Assert.IsFalse(fixture.Runner.Requests.SkipWhile(item => item != lease).Any(item => item.FileName == "cmake"));
        var document = fixture.Document("captures.json");
        Assert.AreEqual(6, document.GetArrayLength());
        CollectionAssert.AreEqual(new[] { "baseline", "candidate", "baseline", "candidate", "candidate", "baseline" },
            document.EnumerateArray().Select(item => item.GetProperty("repetition").GetProperty("side").GetString()).ToArray());
        Assert.AreEqual(6, document.EnumerateArray().Select(item => item.GetProperty("runId").GetString()).Distinct().Count());
        var comparison = fixture.Document("comparison.json");
        Assert.IsTrue(comparison.GetProperty("comparable").GetBoolean());
        Assert.AreEqual(2, comparison.GetProperty("metrics")[0].GetProperty("baseline").GetProperty("samples").GetInt32());
        Assert.AreEqual(11, comparison.GetProperty("metrics")[0].GetProperty("candidate").GetProperty("median").GetDouble());
        Assert.IsTrue(fixture.Runner.Requests.Any(item => item.FileName == "git" && item.Arguments.Contains("remove")));
        Assert.IsTrue(fixture.Runner.Requests.Where(item => item.FileName == fixture.Editor).All(item =>
            item.Arguments.Any(arg => arg.StartsWith("-abslog=", StringComparison.Ordinal)) && item.Arguments.Contains("-ForceRes")));
    }

    [DataTestMethod]
    [DataRow("metrics.csv")]
    [DataRow("result.json")]
    [DataRow("capture.utrace")]
    [DataRow("unreal.log")]
    public async Task Missing_artifacts_fail_with_owned_failure_manifests(string missing)
    {
        using var fixture = new Fixture { MissingArtifact = missing };
        Assert.AreEqual(1, await fixture.Run("sandbox-ismc", "--skip-build"));
        Assert.AreEqual("failed", fixture.Document("manifest.json").GetProperty("status").GetString());
        var run_manifest = JsonDocument.Parse(File.ReadAllText(Directory.GetFiles(fixture.Output, "manifest.json", SearchOption.AllDirectories)
            .Single(path => path.Contains(Path.DirectorySeparatorChar + "runs" + Path.DirectorySeparatorChar, StringComparison.Ordinal))));
        Assert.AreEqual("failed", run_manifest.RootElement.GetProperty("status").GetString());
        StringAssert.Contains(run_manifest.RootElement.GetProperty("failure").GetString()!, missing);
    }

    [TestMethod]
    public async Task Observed_viewport_mismatch_fails_and_never_produces_comparison_deltas()
    {
        using var fixture = new Fixture { ViewportMismatch = true };
        Assert.AreEqual(1, await fixture.Run("sandbox-ismc-revision-ab", "--baseline", "old"));
        Assert.AreEqual(1, fixture.Runner.Requests.Count(item => item.FileName == fixture.Editor));
        StringAssert.Contains(fixture.Errors.ToString(), "observed_width");
        Assert.IsFalse(File.Exists(Path.Combine(fixture.RunDirectory, "comparison.json")));
    }

    [TestMethod]
    public async Task Differing_render_conditions_are_incomparable_without_deltas()
    {
        using var fixture = new Fixture { RhiMismatch = true };
        Assert.AreEqual(1, await fixture.Run("sandbox-ismc-revision-ab", "--baseline", "old"));
        var result = fixture.Document("comparison.json");
        Assert.IsFalse(result.GetProperty("comparable").GetBoolean());
        Assert.AreEqual(0, result.GetProperty("metrics").GetArrayLength());
        Assert.AreEqual("incomparable", fixture.Document("manifest.json").GetProperty("status").GetString());
    }

    [TestMethod]
    public async Task Supplied_worktree_is_not_removed_and_skip_build_keeps_one_lease()
    {
        using var fixture = new Fixture();
        var supplied = Path.Combine(fixture.Root, "prepared");
        Fixture.WriteProtocol(supplied);
        Assert.AreEqual(0, await fixture.Run("sandbox-ismc-revision-ab", "--baseline", "old", "--baseline-worktree", supplied, "--skip-build"), fixture.Errors.ToString());
        Assert.IsTrue(Directory.Exists(supplied));
        Assert.IsFalse(fixture.Runner.Requests.Any(item => item.Arguments.Contains("remove") || item.Arguments.Contains("add") || item.FileName == "cmake"));
        Assert.AreEqual(1, fixture.Runner.Requests.Count(item => item.FileName == "test-jobserver"));
    }

    [TestMethod]
    public async Task Build_failure_cleans_owned_baseline_and_records_failure_without_a_lease()
    {
        using var fixture = new Fixture { BuildFailure = true };
        Assert.AreEqual(1, await fixture.Run("sandbox-ismc-revision-ab", "--baseline", "old"));
        Assert.IsTrue(fixture.Runner.Requests.Any(item => item.Arguments.Contains("remove")));
        Assert.IsFalse(fixture.Runner.Requests.Any(item => item.FileName == "test-jobserver"));
        Assert.AreEqual("failed", fixture.Document("manifest.json").GetProperty("status").GetString());
    }

    [DataTestMethod]
    [DataRow("timeout")]
    [DataRow("launch")]
    [DataRow("exit")]
    [DataRow("malformed")]
    public async Task Process_and_protocol_failures_retain_manifests_and_clean_owned_baseline(string failure)
    {
        using var fixture = new Fixture { ProcessFailure = failure };
        Assert.AreEqual(1, await fixture.Run("sandbox-ismc-revision-ab", "--baseline", "old"));
        Assert.AreEqual("failed", fixture.Document("manifest.json").GetProperty("status").GetString());
        Assert.IsTrue(fixture.Runner.Requests.Any(item => item.Arguments.Contains("remove")));
        Assert.AreEqual(1, fixture.Runner.Requests.Count(item => item.FileName == fixture.Editor));
    }

    [TestMethod]
    public async Task Frame_memory_uses_shared_preparation_order_and_one_lease()
    {
        using var fixture = new Fixture();
        Assert.AreEqual(0, await fixture.Run("frame-memory-revision-ab", "--baseline", "old", "--iterations", "2", "--warmup-iterations", "1"), fixture.Errors.ToString());
        Assert.AreEqual(1, fixture.Runner.Requests.Count(item => item.FileName == "test-jobserver"));
        var records = fixture.Document("records.json");
        Assert.AreEqual(4, records.GetArrayLength());
        CollectionAssert.AreEqual(new[] { "baseline", "candidate", "candidate", "baseline" }, records.EnumerateArray().Select(item => item.GetProperty("state").GetString()).ToArray());
        Assert.AreEqual(6, fixture.Runner.Requests.Count(item => item.Arguments.Contains("native-simulation")));
    }

    [TestMethod]
    public async Task Historical_revision_without_protocol_is_rejected_before_build_or_measurement()
    {
        using var fixture = new Fixture { MissingProtocol = true };
        Assert.AreEqual(1, await fixture.Run("sandbox-ismc-revision-ab", "--baseline", "old"));
        StringAssert.Contains(fixture.Errors.ToString(), "lacks the owned-output/viewport protocol");
        Assert.IsFalse(fixture.Runner.Requests.Any(item => item.FileName == "cmake" || item.FileName == "test-jobserver"));
    }

    private sealed class Fixture : IDisposable
    {
        private readonly BenchmarkTestDirectory directory_ = new();
        public string Root => directory_.Root;
        public string Editor { get; }
        public string Output => Path.Combine(Root, "output");
        public string RunDirectory => Directory.GetDirectories(Output).Single();
        public RecordingRunner Runner { get; }
        public StringWriter Errors { get; } = new();
        public string? MissingArtifact { get; init; }
        public bool ViewportMismatch { get; init; }
        public bool RhiMismatch { get; init; }
        public bool BuildFailure { get; init; }
        public bool MissingProtocol { get; init; }
        public string? ProcessFailure { get; init; }

        public Fixture()
        {
            Directory.CreateDirectory(Path.Combine(Root, ".git"));
            File.WriteAllText(Path.Combine(Root, "CMakeLists.txt"), string.Empty);
            Editor = Path.Combine(Root, "UE", "Engine", "Binaries", "Win64", "UnrealEditor-Cmd.exe");
            Directory.CreateDirectory(Path.GetDirectoryName(Editor)!);
            File.WriteAllText(Editor, "editor");
            WriteProtocol(Root);
            Runner = new RecordingRunner(Respond);
        }

        public async Task<int> Run(string command, params string[] args)
        {
            var arguments = new List<string> { command, "--output-dir", Output };
            if (command != "frame-memory-revision-ab") arguments.AddRange(["--editor", Editor]);
            arguments.AddRange(args);
            return await Application(false).RunAsync(arguments, Root);
        }

        public JsonElement Document(string name)
        {
            using var document = JsonDocument.Parse(File.ReadAllText(Path.Combine(RunDirectory, name)));
            return document.RootElement.Clone();
        }

        private BenchmarkToolsApplication Application(bool child) => new(Runner, new TestJobserver(), new TestEnvironment(child ? "lease" : null), TextWriter.Null, Errors, "current-benchmark-tools");

        private async Task<ProcessResult> Respond(ProcessRequest process)
        {
            if (process.FileName == "git")
            {
                var args = process.Arguments.ToList();
                if (args.Contains("--show-toplevel")) return new ProcessResult(0, process.WorkingDirectory);
                if (args.Contains("rev-parse")) return new ProcessResult(0, "commit");
                if (args.Contains("add") && args.Contains("worktree"))
                {
                    var path = args[args.IndexOf("--detach") + 1];
                    Directory.CreateDirectory(path);
                    if (!MissingProtocol) WriteProtocol(path);
                }
                return new ProcessResult(0);
            }
            if (process.FileName == "cmake") return new ProcessResult(BuildFailure ? 1 : 0, StandardError: BuildFailure ? "fixture build failure" : "");
            if (process.FileName == "test-jobserver")
            {
                var index = process.Arguments.ToList().IndexOf("--");
                return new ProcessResult(await Application(true).RunAsync(process.Arguments.Skip(index + 2).ToArray(), Root));
            }
            if (process.Arguments.Contains("native-simulation")) return new ProcessResult(0,
                "{\"timing\":{\"mean_tick_microseconds\":10},\"memory\":{\"frame_peak_claimed_bytes\":1,\"frame_peak_payload_bytes\":1,\"frame_total_padding_bytes\":0,\"frame_total_root_claims\":1}}");
            if (process.FileName != Editor) throw new AssertFailedException("Unexpected executable: " + process.FileName);
            Assert.IsNotNull(process.OutputLogPath);
            File.WriteAllText(process.OutputLogPath, "Unreal process output");
            if (ProcessFailure == "timeout") throw new ProcessTimeoutException("fixture timeout");
            if (ProcessFailure == "launch") throw new ProcessLaunchException("fixture launch", new IOException("missing editor"));
            if (ProcessFailure == "exit") return new ProcessResult(7, StandardError: "fixture process failure");
            string Argument(string name) => process.Arguments.Single(item => item.StartsWith(name + "=", StringComparison.Ordinal))[(name.Length + 1)..];
            var directory = Argument("-SandboxISMCBenchmarkOutput");
            var id = Argument("-SandboxISMCBenchmarkRunId");
            var is_candidate = process.WorkingDirectory == Root;
            var conditions = SandboxIsmcResults.RequiredConditions.ToDictionary(key => key, _ => "0", StringComparer.Ordinal);
            using var plan_document = JsonDocument.Parse(File.ReadAllText(Path.Combine(RunDirectory, "measurement-plan.json")));
            var settings = plan_document.RootElement.GetProperty("ismc").Deserialize<SandboxIsmcRequest>(BenchmarkCommandSupport.JsonOptions)!;
            foreach (var item in settings.Conditions()) conditions[item.Key] = item.Value;
            conditions["result_schema"] = "1";
            conditions["rhi"] = RhiMismatch && is_candidate ? "Vulkan" : "D3D12";
            if (ViewportMismatch) conditions["observed_width"] = "640";
            if (MissingArtifact != "result.json") BenchmarkCommandSupport.WriteJson(Path.Combine(directory, "result.json"), new { SchemaVersion = 1, RunId = id, Complete = true, Conditions = conditions });
            if (ProcessFailure == "malformed") File.WriteAllText(Path.Combine(directory, "result.json"), "{}");
            if (MissingArtifact != "metrics.csv")
            {
                var dimensions = new Dictionary<string, string> { ["renderer"] = "benchmark" };
                foreach (var key in new[] { "mode", "visibility", "bounds", "custom_data", "churn", "min_instances", "half_cycle_updates", "replacement_percent", "warmup_updates", "warmup_seconds", "measurement_seconds", "instances", "update_percent" }) dimensions[key] = conditions[key];
                dimensions["updated_instances"] = conditions["instances"];
                var value = is_candidate ? "11" : "10";
                BenchmarkCommandSupport.WriteCsv(Path.Combine(directory, "metrics.csv"),
                    [dimensions.Keys.Concat(["metric", "unit", "samples", "min", "median", "p95", "max"]).ToArray(),
                     dimensions.Values.Concat(["frame", "ms", "100", value, value, value, value]).ToArray()]);
            }
            foreach (var artifact in new[] { "unreal.log", "capture.utrace" })
                if (MissingArtifact != artifact) File.WriteAllText(Path.Combine(directory, artifact), "fixture artifact");
            Assert.AreEqual(Path.Combine(directory, "unreal.log"), Argument("-abslog"));
            return new ProcessResult(0, "Unreal process output");
        }

        public static void WriteProtocol(string root)
        {
            var path = Path.Combine(root, "Plugins", "SandboxISMC", "Source", "SandboxISMCLab", "Private", "SandboxISMCBenchmarkActor.cpp");
            Directory.CreateDirectory(Path.GetDirectoryName(path)!);
            File.WriteAllText(path, "SandboxISMCBenchmarkRunId=");
        }

        public void Dispose() => directory_.Dispose();
    }

    private sealed class RecordingRunner(Func<ProcessRequest, Task<ProcessResult>> callback) : IProcessRunner
    {
        public List<ProcessRequest> Requests { get; } = [];
        public Task<ProcessResult> RunAsync(ProcessRequest request, CancellationToken token)
        {
            Requests.Add(request);
            return callback(request);
        }
    }
}
