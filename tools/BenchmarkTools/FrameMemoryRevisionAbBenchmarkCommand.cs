using System.Globalization;
using System.Text.Json;

namespace BenchmarkTools;

internal static class FrameMemoryRevisionAbBenchmarkCommand
{
    private static readonly HashSet<string> value_arguments = ["--iterations", "--warmup-iterations", "--baseline", "--output-dir", "--baseline-worktree"];
    private static readonly HashSet<string> flag_arguments = ["--skip-build", "--prepare-only", "--validate-only", "--keep-baseline-worktree"];

    public static async Task<int> RunAsync(BenchmarkToolsApplication application, RepositoryPaths repository_paths, IReadOnlyList<string> arguments, CancellationToken cancellation_token)
    {
        if (arguments.Count == 2 && arguments[0] == "--measurement-plan")
        {
            var plan = await BenchmarkMeasurement.ReadAsync(application, arguments[1], cancellation_token);
            if (plan.Ismc is not null) throw new BenchmarkToolException("Frame-memory plan contains an unrelated workload.");
            var records = new List<Record>();
            foreach (var item in plan.Sequence)
            {
                var source = item.Side == "baseline" ? plan.Baseline : plan.Candidate;
                var record = await InvokeAsync(application, source.Root, item.Warmup ? 0 : item.Repetition, item.Sequence, item.Side, source.Commit, cancellation_token);
                if (!item.Warmup) records.Add(record);
                BenchmarkCommandSupport.WriteJson(Path.Combine(plan.Output, "records.json"), records);
            }
            return 0;
        }
        var parsed = CommandArguments.Parse(arguments, value_arguments, flag_arguments);
        var iterations = parsed.PositiveInt32("--iterations", 5, 100);
        var warmups = parsed.NonnegativeInt32("--warmup-iterations", 0, 10);
        var provided = parsed.Value("--baseline-worktree", string.Empty);
        if (parsed.HasFlag("--skip-build") && provided.Length == 0) throw new BenchmarkToolException("'--skip-build' requires '--baseline-worktree'.");
        var run = BenchmarkRunContext.Create(repository_paths, "frame-memory-revision-ab", parsed.Value("--output-dir", ".local/benchmarks/frame-memory-revision-ab"),
            new { Iterations = iterations, WarmupIterations = warmups, Seconds = 20, GameSpeed = 100 });
        application.StandardOutput.WriteLine($"Artifacts: {run.DirectoryPath}");
        try
        {
            await using var revisions = await RevisionComparisonSession.CreateAsync(application, repository_paths, parsed.Value("--baseline", "HEAD"),
                run.Manifest.RunId, provided.Length == 0 ? null : provided, parsed.HasFlag("--keep-baseline-worktree"), cancellation_token, run.DirectoryPath);
            run.Manifest.Provenance = new { revisions.Candidate, revisions.Baseline, revisions.OwnsBaseline, Orchestrator = application.ExecutablePath, EffectiveArguments = arguments };
            run.Publish();
            if (!parsed.HasFlag("--skip-build"))
            {
                await BuildAsync(application, revisions.Candidate.Root, false, cancellation_token);
                await BuildAsync(application, revisions.BaselineRoot, revisions.OwnsBaseline, cancellation_token);
            }
            if (parsed.HasFlag("--prepare-only"))
            {
                await BenchmarkRunContext.VerifySourceAsync(application, revisions.Candidate, cancellation_token);
                await BenchmarkRunContext.VerifySourceAsync(application, revisions.Baseline, cancellation_token);
                run.Complete();
                return 0;
            }
            var sequence = BenchmarkOrdering.Balanced(parsed.HasFlag("--validate-only") ? 1 : iterations, parsed.HasFlag("--validate-only") ? 0 : warmups);
            BenchmarkCommandSupport.WriteJson(run.Artifact("sequence.json"), sequence);
            run.Manifest.Artifacts["sequence.json"] = run.Artifact("sequence.json");
            run.Manifest.Artifacts["measurement-plan.json"] = run.Artifact("measurement-plan.json");
            run.Expect("records.json");
            run.Manifest.Status = "measuring";
            run.Publish();
            await BenchmarkMeasurement.RunAsync(application, repository_paths, "frame-memory-revision-ab",
                new BenchmarkMeasurementPlan(run.DirectoryPath, revisions.Candidate, revisions.Baseline, sequence), cancellation_token);
            run.ValidateArtifacts();
            var records = JsonSerializer.Deserialize<List<Record>>(File.ReadAllText(run.Artifact("records.json")), BenchmarkCommandSupport.JsonOptions)
                ?? throw new BenchmarkToolException("Frame-memory results were empty.");
            var paired = Pair(records, parsed.HasFlag("--validate-only") ? 1 : iterations);
            BenchmarkCommandSupport.WriteCsv(run.Artifact("raw-results.csv"), RawRows(records));
            BenchmarkCommandSupport.WriteCsv(run.Artifact("paired-results.csv"), PairedRows(paired));
            run.Expect("raw-results.csv");
            run.Expect("paired-results.csv");
            application.StandardOutput.WriteLine($"native: mean delta {paired.Average(record => record.DeltaPercent):N3}%, median delta {Median(paired.Select(record => record.DeltaPercent)):N3}%");
            run.Complete();
            return 0;
        }
        catch (Exception error)
        {
            run.Fail(error);
            throw;
        }
    }

    private static async Task BuildAsync(BenchmarkToolsApplication application, string root, bool owned, CancellationToken token)
    {
        if (owned) await BenchmarkGit.TextAsync(application, root, ["submodule", "update", "--init", "--depth", "1", "native/third_party/googletest", "native/third_party/cpu_features", "native/third_party/tracy", "native/third_party/cli11"], token);
        await BenchmarkGit.SuccessAsync(application, new ProcessRequest("cmake", ["--preset", RepositoryPaths.NativeSimulationConfigurePreset("frame-memory-level-benchmark")], root), token);
        await BenchmarkGit.SuccessAsync(application, new ProcessRequest("cmake", ["--build", "--preset", "frame-memory-level-benchmark", "--target", "native-simulation-benchmark"], root), token);
    }

    private static async Task<Record> InvokeAsync(BenchmarkToolsApplication application, string root, int pair, int sequence, string state, string commit, CancellationToken cancellation_token)
    {
        var command = new[] { "native-simulation", "--level", Path.Combine(root, "LevelScripts", "Benchmarks", "Batch_benchmark.scm"), "--seconds", "20", "--game-speed", "100", "--build-preset", "frame-memory-level-benchmark", "--skip-build" };
        var process = await application.ProcessRunner.RunAsync(new ProcessRequest(application.ExecutablePath, command, root), cancellation_token);
        if (process.ExitCode != 0)
        {
            application.WriteProcessOutput(process);
            throw new BenchmarkToolException($"NativeFrameMemoryLevel revision comparison failed with exit code {process.ExitCode}.");
        }
        var results = BenchmarkCommandSupport.JsonLines(process.StandardOutput).ToArray();
        if (results.Length != 1) throw new BenchmarkToolException($"NativeFrameMemoryLevel produced {results.Length} JSON results.");
        try
        {
            var timing = results[0].GetProperty("timing");
            var memory = results[0].GetProperty("memory");
            var mean_tick = timing.GetProperty("mean_tick_microseconds").GetDouble();
            if (!double.IsFinite(mean_tick) || mean_tick <= 0)
            {
                throw new BenchmarkToolException("NativeFrameMemoryLevel result contained an invalid mean tick time.");
            }
            return new Record(pair, sequence, state, commit, mean_tick, memory.GetProperty("frame_peak_claimed_bytes").GetUInt64(), memory.GetProperty("frame_peak_payload_bytes").GetUInt64(), memory.GetProperty("frame_total_padding_bytes").GetUInt64(), memory.GetProperty("frame_total_root_claims").GetUInt64());
        }
        catch (Exception exception) when (exception is KeyNotFoundException or InvalidOperationException or FormatException)
        {
            throw new BenchmarkToolException($"NativeFrameMemoryLevel result was malformed: {exception.Message}");
        }
    }

    private static List<Paired> Pair(IReadOnlyList<Record> records, int iterations) { var result = new List<Paired>(); for (var pair = 1; pair <= iterations; ++pair) { var baseline = records.SingleOrDefault(record => record.Pair == pair && record.State == "baseline"); var candidate = records.SingleOrDefault(record => record.Pair == pair && record.State == "candidate"); if (baseline is null || candidate is null) throw new BenchmarkToolException($"Pair {pair} did not contain one baseline and candidate result for native."); var delta = candidate.MeanTickUs - baseline.MeanTickUs; result.Add(new Paired(pair, baseline.MeanTickUs, candidate.MeanTickUs, delta, 100 * delta / baseline.MeanTickUs)); } return result; }
    private static IEnumerable<string[]> RawRows(IEnumerable<Record> records) { yield return ["pair", "sequence", "state", "commit", "variant", "mean_tick_us", "peak_claimed_bytes", "peak_payload_bytes", "total_padding_bytes", "total_root_claims"]; foreach (var record in records) yield return [record.Pair.ToString(CultureInfo.InvariantCulture), record.Sequence.ToString(CultureInfo.InvariantCulture), record.State, record.Commit, "native", record.MeanTickUs.ToString("R", CultureInfo.InvariantCulture), record.PeakClaimedBytes.ToString(CultureInfo.InvariantCulture), record.PeakPayloadBytes.ToString(CultureInfo.InvariantCulture), record.TotalPaddingBytes.ToString(CultureInfo.InvariantCulture), record.TotalRootClaims.ToString(CultureInfo.InvariantCulture)]; }
    private static IEnumerable<string[]> PairedRows(IEnumerable<Paired> records) { yield return ["pair", "variant", "baseline_tick_us", "candidate_tick_us", "delta_tick_us", "delta_percent"]; foreach (var record in records) yield return [record.Pair.ToString(CultureInfo.InvariantCulture), "native", record.BaselineTickUs.ToString("R", CultureInfo.InvariantCulture), record.CandidateTickUs.ToString("R", CultureInfo.InvariantCulture), record.DeltaTickUs.ToString("R", CultureInfo.InvariantCulture), record.DeltaPercent.ToString("R", CultureInfo.InvariantCulture)]; }
    private static double Median(IEnumerable<double> values) { var items = values.Order().ToArray(); return items.Length % 2 == 1 ? items[items.Length / 2] : (items[items.Length / 2 - 1] + items[items.Length / 2]) / 2; }
    private sealed record Record(int Pair, int Sequence, string State, string Commit, double MeanTickUs, ulong PeakClaimedBytes, ulong PeakPayloadBytes, ulong TotalPaddingBytes, ulong TotalRootClaims);
    private sealed record Paired(int Pair, double BaselineTickUs, double CandidateTickUs, double DeltaTickUs, double DeltaPercent);
}
