using System.Text.Json;

namespace BenchmarkTools;

internal static class SandboxIsmcBenchmarkCommand
{
    public static async Task<int> RunAsync(BenchmarkToolsApplication application, RepositoryPaths repository, IReadOnlyList<string> arguments,
        bool comparison, CancellationToken token)
    {
        if (arguments.Count == 2 && arguments[0] == "--measurement-plan")
        {
            var plan = BenchmarkMeasurement.Read(application, arguments[1]);
            var request = plan.Ismc ?? throw new BenchmarkToolException("SandboxISMC measurement plan has no workload.");
            var captures = new List<SandboxIsmcCapture>();
            foreach (var repetition in plan.Sequence)
            {
                var source = repetition.Side == "baseline" ? plan.Baseline : plan.Candidate;
                var run = BenchmarkRunContext.Create(new RepositoryPaths(source.Root), "sandbox-ismc", Path.Combine(plan.Output, "runs"), request);
                run.Manifest.Provenance = new { Source = source, Repetition = repetition, request.Editor, Arguments = request.EditorArguments(source.Root, run) };
                foreach (var artifact in new[] { "metrics.csv", "result.json", "unreal.log", "process.log" }) run.Expect(artifact);
                if (request.Trace) run.Expect("capture.utrace");
                try
                {
                    run.Manifest.Status = "measuring";
                    run.Publish();
                    var process = await application.ProcessRunner.RunAsync(new ProcessRequest(request.Editor,
                        request.EditorArguments(source.Root, run), source.Root, Timeout: TimeSpan.FromSeconds(request.Seconds + request.WarmupSeconds + 180)), token);
                    File.WriteAllText(run.Artifact("process.log"), process.StandardOutput + process.StandardError);
                    if (process.ExitCode != 0) throw new BenchmarkToolException($"Unreal exited with code {process.ExitCode}; see '{run.Artifact("unreal.log")}'.");
                    run.ValidateArtifacts();
                    captures.Add(SandboxIsmcResults.Read(run, request, repetition));
                    run.Complete();
                    BenchmarkCommandSupport.WriteJson(Path.Combine(plan.Output, "captures.json"), captures);
                }
                catch (Exception error)
                {
                    run.Fail(error);
                    throw;
                }
            }
            return 0;
        }

        var value_arguments = new HashSet<string>(SandboxIsmcRequest.ValueArguments) { "--compatibility" };
        var parsed = CommandArguments.Parse(arguments, value_arguments, new HashSet<string> { "--skip-build", "--keep-baseline-worktree" });
        var settings = SandboxIsmcRequest.Parse(parsed, repository);
        var repetitions = parsed.PositiveInt32("--repetitions", comparison ? 2 : 1, 100);
        var warmups = parsed.NonnegativeInt32("--warmup-runs", 0, 10);
        if (!comparison && (arguments.Any(arg => arg.StartsWith("--baseline", StringComparison.Ordinal)) || repetitions != 1 || warmups != 0 || parsed.Value("--compatibility", "none") != "none"))
            throw new BenchmarkToolException("Revision and repetition options require sandbox-ismc-revision-ab.");
        var supplied = parsed.Value("--baseline-worktree", string.Empty);
        if (comparison && parsed.HasFlag("--skip-build") && supplied.Length == 0)
            throw new BenchmarkToolException("--skip-build requires --baseline-worktree for revision comparisons.");
        var command = comparison ? "sandbox-ismc-revision-ab" : "sandbox-ismc";
        var context = BenchmarkRunContext.Create(repository, command, parsed.Value("--output-dir", $".local/benchmarks/{command}"), settings);
        application.StandardOutput.WriteLine($"Artifacts: {context.DirectoryPath}");
        try
        {
            await using var revisions = comparison ? await RevisionComparisonSession.CreateAsync(application, repository,
                parsed.Required("--baseline"), context.Manifest.RunId, supplied.Length == 0 ? null : supplied,
                parsed.HasFlag("--keep-baseline-worktree"), token) : null;
            var candidate = revisions?.Candidate ?? await BenchmarkRunContext.SourceAsync(application, repository.Root, token);
            var baseline = revisions?.Baseline ?? candidate;
            context.Manifest.Provenance = new { Candidate = candidate, Baseline = comparison ? baseline : null,
                BaselineOwned = revisions?.OwnsBaseline, Compatibility = parsed.Value("--compatibility", "none"),
                Orchestrator = application.ExecutablePath, EffectiveArguments = arguments };
            context.Publish();
            RequireProtocol(candidate.Root);
            if (revisions is not null)
            {
                await SandboxIsmcCompatibility.ApplyAsync(application, revisions, parsed.Value("--compatibility", "none"), context, token);
                RequireProtocol(baseline.Root);
                baseline = await BenchmarkRunContext.SourceAsync(application, baseline.Root, token);
                context.Manifest.Provenance = new { Candidate = candidate, Baseline = revisions.Baseline, EffectiveBaseline = baseline,
                    BaselineOwned = revisions.OwnsBaseline, Compatibility = parsed.Value("--compatibility", "none"),
                    Orchestrator = application.ExecutablePath, EffectiveArguments = arguments };
                context.Publish();
            }
            if (!parsed.HasFlag("--skip-build"))
            {
                await BuildAsync(application, candidate.Root, settings.Editor, false, token);
                if (comparison) await BuildAsync(application, baseline.Root, settings.Editor, revisions!.OwnsBaseline, token);
            }
            var sequence = comparison ? BenchmarkOrdering.Balanced(repetitions, warmups) : [new BenchmarkRepetition(1, 1, "candidate", false)];
            context.Manifest.Artifacts["sequence.json"] = context.Artifact("sequence.json");
            context.Manifest.Artifacts["measurement-plan.json"] = context.Artifact("measurement-plan.json");
            BenchmarkCommandSupport.WriteJson(context.Artifact("sequence.json"), sequence);
            context.Expect("captures.json");
            context.Manifest.Status = "measuring";
            context.Publish();
            await BenchmarkMeasurement.RunAsync(application, repository, command,
                new BenchmarkMeasurementPlan(context.DirectoryPath, candidate, baseline, sequence, settings), token);
            context.ValidateArtifacts();
            var captures = JsonSerializer.Deserialize<List<SandboxIsmcCapture>>(File.ReadAllText(context.Artifact("captures.json")), BenchmarkCommandSupport.JsonOptions)
                ?? throw new BenchmarkToolException("Missing captures.");
            if (captures.Count != sequence.Count || !captures.Select(item => item.Repetition).SequenceEqual(sequence))
                throw new BenchmarkToolException("Incomplete or reordered comparison measurements.");
            context.Manifest.Comparability = captures[0].Conditions;
            if (comparison)
            {
                var results = SandboxIsmcResults.Compare(captures);
                SandboxIsmcResults.WriteReport(context.DirectoryPath, results);
                foreach (var artifact in new[] { "comparison.json", "comparison.csv", "comparison.md" }) context.Expect(artifact);
                if (!results.Comparable)
                {
                    context.Manifest.Status = "incomparable";
                    context.Manifest.Failure = string.Join("; ", results.Errors);
                    context.Publish();
                    return 1;
                }
            }
            context.Complete();
            return 0;
        }
        catch (Exception error)
        {
            context.Fail(error);
            throw;
        }
    }

    internal static void RequireProtocol(string root)
    {
        var path = Path.Combine(root, "Plugins", "SandboxISMC", "Source", "SandboxISMCLab", "Private", "SandboxISMCBenchmarkActor.cpp");
        if (!File.Exists(path) || !File.ReadAllText(path).Contains("SandboxISMCBenchmarkRunId=", StringComparison.Ordinal))
            throw new BenchmarkToolException($"SandboxISMC in '{root}' lacks the owned-output/viewport protocol. Use an explicitly supported --compatibility overlay for a historical baseline.");
    }

    internal static async Task BuildAsync(BenchmarkToolsApplication application, string root, string editor, bool owned, CancellationToken token)
    {
        if (owned) await BenchmarkGit.TextAsync(application, root, ["submodule", "update", "--init", "--recursive"], token);
        var engine_root = Directory.GetParent(editor)!.Parent!.Parent!.Parent!.FullName;
        await BenchmarkGit.SuccessAsync(application, new ProcessRequest("cmake", ["--preset", "sandbox-ismc-benchmark", $"-DUE_ROOT={engine_root}"], root), token);
        await BenchmarkGit.SuccessAsync(application, new ProcessRequest("cmake", ["--build", "--preset", "sandbox-ismc-benchmark", "--target", "editor"], root), token);
    }
}
