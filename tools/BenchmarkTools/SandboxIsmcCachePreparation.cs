using System.Text.Json;

namespace BenchmarkTools;

internal static class SandboxIsmcCachePreparation
{
    internal static readonly TimeSpan Timeout = TimeSpan.FromMinutes(10);
    private sealed record Plan(int SchemaVersion, RevisionIdentity Source, SandboxIsmcRequest Request, string Output);

    public static async Task RunAsync(BenchmarkToolsApplication application, RepositoryPaths repository, BenchmarkRunContext comparison,
        RevisionIdentity source, SandboxIsmcRequest request, string side, CancellationToken token)
    {
        var run = BenchmarkRunContext.Create(repository, "sandbox-ismc-cache", Path.Combine(comparison.DirectoryPath, "preparation"),
            new { Workload = request, TimeoutSeconds = Timeout.TotalSeconds }, publish_latest: false);
        run.Manifest.Purpose = "cache-preparation";
        run.Manifest.Provenance = new { Source = source, Side = side, Arguments = request.CacheArguments(source.Root, run.Artifact("unreal.log")) };
        run.Expect("process.log");
        run.Expect("unreal.log");
        run.Expect("cache-plan.json");
        BenchmarkCommandSupport.WriteJson(run.Artifact("cache-plan.json"), new Plan(1, source, request, run.DirectoryPath));
        comparison.Manifest.Artifacts[$"cache-{side}"] = run.Artifact("manifest.json");
        comparison.Publish();
        application.StandardOutput.WriteLine($"Preparing {side} shader/cache data (up to {Timeout.TotalMinutes:0} minutes): {run.DirectoryPath}");
        try
        {
            var arguments = new List<string> { "run", "--name", $"SandboxISMC {side} cache preparation", "--kind", "build",
                "--worktree", repository.Root, "--shared", "machine", "--shared", BenchmarkCommandSupport.EngineResource(request.Editor),
                "--", application.ExecutablePath, "sandbox-ismc", "--prepare-cache-plan", run.Artifact("cache-plan.json") };
            // The child owns the timeout so waiting for shared resources does not consume preparation time.
            var result = await application.ProcessRunner.RunAsync(new ProcessRequest(application.JobserverLocator.Locate(), arguments, repository.Root), token);
            application.WriteProcessOutput(result);
            if (result.ExitCode != 0) throw new BenchmarkToolException($"SandboxISMC {side} cache preparation failed: {result.StandardError.Trim()} See '{run.DirectoryPath}'.");
            run.ValidateArtifacts();
            run.Complete();
        }
        catch (Exception error)
        {
            run.Fail(error);
            throw;
        }
    }

    public static async Task<int> RunChildAsync(BenchmarkToolsApplication application, string path, CancellationToken token)
    {
        if (string.IsNullOrWhiteSpace(application.Environment.GetEnvironmentVariable("NUKETHEBEES_JOBSERVER_JOB")))
            throw new BenchmarkToolException("Cache preparation requires a jobserver reservation.");
        var plan = JsonSerializer.Deserialize<Plan>(File.ReadAllText(path), BenchmarkCommandSupport.JsonOptions)
            ?? throw new BenchmarkToolException("Cache preparation plan is empty.");
        if (plan.SchemaVersion != 1) throw new BenchmarkToolException("Unsupported cache preparation plan schema.");
        await BenchmarkRunContext.VerifySourceAsync(application, plan.Source, token);
        if (!File.Exists(SandboxIsmcRequest.MapPath(plan.Source.Root))) throw new BenchmarkToolException("SandboxISMC cache preparation map is missing.");
        var log = Path.Combine(plan.Output, "unreal.log");
        var process_log = Path.Combine(plan.Output, "process.log");
        try
        {
            var result = await application.ProcessRunner.RunAsync(new ProcessRequest(plan.Request.Editor,
                plan.Request.CacheArguments(plan.Source.Root, log), plan.Source.Root, Timeout: Timeout, OutputLogPath: process_log), token);
            if (result.ExitCode != 0) throw new BenchmarkToolException($"Unreal cache preparation exited with code {result.ExitCode}; see '{log}' and '{process_log}'.");
        }
        catch (ProcessTimeoutException)
        {
            throw new BenchmarkToolException($"Unreal shader/cache preparation timed out after {Timeout.TotalMinutes:0} minutes before measurement; see '{log}' and '{process_log}'.");
        }
        await BenchmarkRunContext.VerifySourceAsync(application, plan.Source, token);
        return 0;
    }
}
