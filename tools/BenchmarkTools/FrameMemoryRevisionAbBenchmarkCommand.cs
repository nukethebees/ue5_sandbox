using System.Globalization;
using System.Text;
using System.Text.Json;

namespace BenchmarkTools;

internal static class FrameMemoryRevisionAbBenchmarkCommand
{
    private static readonly HashSet<string> value_arguments = ["--iterations", "--warmup-iterations", "--baseline", "--output-dir", "--baseline-worktree"];
    private static readonly HashSet<string> flag_arguments = ["--skip-build", "--prepare-only", "--validate-only", "--keep-baseline-worktree"];

    public static async Task<int> RunAsync(BenchmarkToolsApplication application, RepositoryPaths repository_paths, IReadOnlyList<string> arguments, CancellationToken cancellation_token)
    {
        var parsed = CommandArguments.Parse(arguments, value_arguments, flag_arguments);
        var iterations = parsed.PositiveInt32("--iterations", 5, 100);
        if (!int.TryParse(parsed.Value("--warmup-iterations", "0"), NumberStyles.None, CultureInfo.InvariantCulture, out var warmups) || warmups is < 0 or > 10)
        {
            throw new BenchmarkToolException("'--warmup-iterations' must be in the range 0 to 10.");
        }
        var provided_worktree = parsed.Value("--baseline-worktree", string.Empty);
        if (parsed.HasFlag("--skip-build") && string.IsNullOrWhiteSpace(provided_worktree)) throw new BenchmarkToolException("'--skip-build' requires '--baseline-worktree'.");
        var baseline_commit = await GitTextAsync(application, repository_paths.Root, ["rev-parse", "--verify", parsed.Value("--baseline", "HEAD") + "^{commit}"], cancellation_token, "Could not resolve baseline revision");
        var candidate_commit = await GitTextAsync(application, repository_paths.Root, ["rev-parse", "HEAD"], cancellation_token, "Could not resolve candidate HEAD commit");
        var output = BenchmarkCommandSupport.ResolveOutputDirectory(repository_paths, parsed.Value("--output-dir", $".local/benchmarks/frame-memory-revision-ab/{DateTime.Now:yyyyMMdd-HHmmss}"));
        Directory.CreateDirectory(output);
        var parent = Path.Combine(repository_paths.Root, ".local", "abw");
        Directory.CreateDirectory(parent);
        var worktree = string.IsNullOrWhiteSpace(provided_worktree) ? Path.Combine(parent, Guid.NewGuid().ToString("N")[..8]) : Path.GetFullPath(provided_worktree);
        if (string.IsNullOrWhiteSpace(provided_worktree) && !IsUnder(worktree, parent)) throw new BenchmarkToolException($"Refusing to create a benchmark worktree outside '{parent}'.");

        var worktree_added = false;
        try
        {
            if (!string.IsNullOrWhiteSpace(provided_worktree))
            {
                if (!Directory.Exists(worktree)) throw new BenchmarkToolException($"Prepared baseline worktree does not exist: '{worktree}'.");
                var prepared = await GitTextAsync(application, worktree, ["rev-parse", "HEAD"], cancellation_token, "Could not resolve prepared baseline worktree");
                if (!string.Equals(prepared, baseline_commit, StringComparison.Ordinal)) throw new BenchmarkToolException($"Prepared worktree is not at baseline commit {baseline_commit}: '{worktree}'.");
            }
            else
            {
                await GitSuccessAsync(application, repository_paths.Root, ["-C", repository_paths.Root, "worktree", "add", "--detach", worktree, baseline_commit], cancellation_token);
                worktree_added = true;
            }

            if (!parsed.HasFlag("--skip-build"))
            {
                application.StandardOutput.WriteLine("Building candidate working tree...");
                await BuildAsync(application, repository_paths.Root, cancellation_token);
                application.StandardOutput.WriteLine($"Building baseline {baseline_commit}...");
                await BuildAsync(application, worktree, cancellation_token);
            }
            if (parsed.HasFlag("--prepare-only"))
            {
                application.StandardOutput.WriteLine($"Candidate and baseline benchmark builds are ready. Baseline: {worktree}");
                return 0;
            }
            if (parsed.HasFlag("--validate-only"))
            {
                _ = await InvokeAsync(application, worktree, 0, 1, "baseline", baseline_commit, cancellation_token);
                _ = await InvokeAsync(application, repository_paths.Root, 0, 2, "candidate", candidate_commit, cancellation_token);
                application.StandardOutput.WriteLine("Candidate and baseline benchmark dry runs passed.");
                return 0;
            }

            var records = new List<Record>();
            var sequence = 0;
            foreach (var state in new[] { ("baseline", worktree, baseline_commit), ("candidate", repository_paths.Root, candidate_commit) })
            {
                for (var warmup = 1; warmup <= warmups; ++warmup)
                {
                    ++sequence;
                    application.StandardOutput.WriteLine($"{state.Item1} warmup {warmup} of {warmups}");
                    _ = await InvokeAsync(application, state.Item2, 0, sequence, state.Item1, state.Item3, cancellation_token);
                }
                for (var pair = 1; pair <= iterations; ++pair)
                {
                    ++sequence;
                    application.StandardOutput.WriteLine($"{state.Item1} iteration {pair} of {iterations}");
                    records.Add(await InvokeAsync(application, state.Item2, pair, sequence, state.Item1, state.Item3, cancellation_token));
                }
            }
            var paired = Pair(records, iterations);
            BenchmarkCommandSupport.WriteCsv(Path.Combine(output, "raw-results.csv"), RawRows(records));
            BenchmarkCommandSupport.WriteCsv(Path.Combine(output, "paired-results.csv"), PairedRows(paired));
            application.StandardOutput.WriteLine($"Results written to {output}");
            application.StandardOutput.WriteLine($"native: mean delta {paired.Average(record => record.DeltaPercent):N3}%, median delta {Median(paired.Select(record => record.DeltaPercent)):N3}%");
            return 0;
        }
        finally
        {
            if (worktree_added && !parsed.HasFlag("--keep-baseline-worktree"))
            {
                await GitSuccessAsync(application, repository_paths.Root, ["-C", repository_paths.Root, "worktree", "remove", "--force", worktree], CancellationToken.None);
                await GitSuccessAsync(application, repository_paths.Root, ["-C", repository_paths.Root, "worktree", "prune"], CancellationToken.None);
            }
        }
    }

    private static async Task BuildAsync(BenchmarkToolsApplication application, string root, CancellationToken cancellation_token)
    {
        await GitSuccessAsync(application, root, ["-C", root, "submodule", "update", "--init", "--depth", "1", "native/third_party/googletest", "native/third_party/cpu_features", "native/third_party/tracy", "native/third_party/cli11"], cancellation_token);
        await ProcessSuccessAsync(application, new ProcessRequest("cmake", ["--preset", RepositoryPaths.NativeSimulationConfigurePreset("frame-memory-level-benchmark")], root), cancellation_token);
        await ProcessSuccessAsync(application, new ProcessRequest("cmake", ["--build", "--preset", "frame-memory-level-benchmark"], root), cancellation_token);
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

    private static async Task<string> GitTextAsync(BenchmarkToolsApplication application, string root, IReadOnlyList<string> arguments, CancellationToken token, string failure)
    {
        var result = await application.ProcessRunner.RunAsync(GitRequest(root, arguments), token);
        if (result.ExitCode != 0) throw new BenchmarkToolException($"{failure}: {result.StandardError.Trim()}");
        return result.StandardOutput.Trim();
    }
    private static async Task GitSuccessAsync(BenchmarkToolsApplication application, string root, IReadOnlyList<string> arguments, CancellationToken token) => await ProcessSuccessAsync(application, GitRequest(root, arguments), token);
    private static async Task ProcessSuccessAsync(BenchmarkToolsApplication application, ProcessRequest request, CancellationToken token) { var result = await application.ProcessRunner.RunAsync(request, token); application.WriteProcessOutput(result); if (result.ExitCode != 0) throw new BenchmarkToolException($"'{request.FileName}' failed with exit code {result.ExitCode}: {result.StandardError.Trim()}"); }
    private static bool IsUnder(string path, string parent) { var relative = Path.GetRelativePath(Path.GetFullPath(parent), Path.GetFullPath(path)); return !Path.IsPathRooted(relative) && relative != ".." && !relative.StartsWith(".." + Path.DirectorySeparatorChar, StringComparison.Ordinal); }
    private static ProcessRequest GitRequest(string root, IReadOnlyList<string> arguments)
    {
        var safe_arguments = new List<string>
        {
            "--no-pager", "--literal-pathspecs", "-c", "core.protectNTFS=true", "-c", "core.protectHFS=true",
            "-c", "core.hooksPath=NUL", "-c", "gc.auto=0", "-c", "maintenance.auto=false", "-c", "submodule.recurse=false",
        };
        safe_arguments.AddRange(arguments);
        return new ProcessRequest("git", safe_arguments, root, new Dictionary<string, string?>
        {
            ["GIT_TERMINAL_PROMPT"] = "0",
            ["GCM_INTERACTIVE"] = "Never",
            ["GIT_EDITOR"] = "false",
            ["GIT_SEQUENCE_EDITOR"] = "false",
        }, TimeSpan.FromMinutes(30));
    }
    private static List<Paired> Pair(IReadOnlyList<Record> records, int iterations) { var result = new List<Paired>(); for (var pair = 1; pair <= iterations; ++pair) { var baseline = records.SingleOrDefault(record => record.Pair == pair && record.State == "baseline"); var candidate = records.SingleOrDefault(record => record.Pair == pair && record.State == "candidate"); if (baseline is null || candidate is null) throw new BenchmarkToolException($"Pair {pair} did not contain one baseline and candidate result for native."); var delta = candidate.MeanTickUs - baseline.MeanTickUs; result.Add(new Paired(pair, baseline.MeanTickUs, candidate.MeanTickUs, delta, 100 * delta / baseline.MeanTickUs)); } return result; }
    private static IEnumerable<string[]> RawRows(IEnumerable<Record> records) { yield return ["pair", "sequence", "state", "commit", "variant", "mean_tick_us", "peak_claimed_bytes", "peak_payload_bytes", "total_padding_bytes", "total_root_claims"]; foreach (var record in records) yield return [record.Pair.ToString(CultureInfo.InvariantCulture), record.Sequence.ToString(CultureInfo.InvariantCulture), record.State, record.Commit, "native", record.MeanTickUs.ToString("R", CultureInfo.InvariantCulture), record.PeakClaimedBytes.ToString(CultureInfo.InvariantCulture), record.PeakPayloadBytes.ToString(CultureInfo.InvariantCulture), record.TotalPaddingBytes.ToString(CultureInfo.InvariantCulture), record.TotalRootClaims.ToString(CultureInfo.InvariantCulture)]; }
    private static IEnumerable<string[]> PairedRows(IEnumerable<Paired> records) { yield return ["pair", "variant", "baseline_tick_us", "candidate_tick_us", "delta_tick_us", "delta_percent"]; foreach (var record in records) yield return [record.Pair.ToString(CultureInfo.InvariantCulture), "native", record.BaselineTickUs.ToString("R", CultureInfo.InvariantCulture), record.CandidateTickUs.ToString("R", CultureInfo.InvariantCulture), record.DeltaTickUs.ToString("R", CultureInfo.InvariantCulture), record.DeltaPercent.ToString("R", CultureInfo.InvariantCulture)]; }
    private static double Median(IEnumerable<double> values) { var items = values.Order().ToArray(); return items.Length % 2 == 1 ? items[items.Length / 2] : (items[items.Length / 2 - 1] + items[items.Length / 2]) / 2; }
    private sealed record Record(int Pair, int Sequence, string State, string Commit, double MeanTickUs, ulong PeakClaimedBytes, ulong PeakPayloadBytes, ulong TotalPaddingBytes, ulong TotalRootClaims);
    private sealed record Paired(int Pair, double BaselineTickUs, double CandidateTickUs, double DeltaTickUs, double DeltaPercent);
}
