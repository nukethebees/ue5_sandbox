using System.Text.Json;

namespace BenchmarkTools;

// The parent owns preparation, analysis and cleanup. Only this immutable plan crosses the lease boundary.
internal sealed record BenchmarkMeasurementPlan(string Output, RevisionIdentity Candidate, RevisionIdentity Baseline,
    IReadOnlyList<BenchmarkRepetition> Sequence, SandboxIsmcRequest? Ismc = null);

internal static class BenchmarkMeasurement
{
    public static async Task RunAsync(BenchmarkToolsApplication application, RepositoryPaths repository, string command,
        BenchmarkMeasurementPlan plan, CancellationToken token)
    {
        var path = Path.Combine(plan.Output, "measurement-plan.json");
        BenchmarkCommandSupport.WriteJson(path, plan);
        var request = JobserverExecution.CreateRequest(application.JobserverLocator.Locate(), application.ExecutablePath,
            repository, command + " complete comparison", [command, "--measurement-plan", path],
            plan.Ismc is null ? null : [BenchmarkCommandSupport.EngineResource(plan.Ismc.Editor)]);
        var result = await application.ProcessRunner.RunAsync(request, token);
        application.WriteProcessOutput(result);
        if (result.ExitCode != 0) throw new BenchmarkToolException($"Measurement job failed with exit code {result.ExitCode}; see '{plan.Output}'.");
    }

    public static BenchmarkMeasurementPlan Read(BenchmarkToolsApplication application, string path)
    {
        if (string.IsNullOrWhiteSpace(application.Environment.GetEnvironmentVariable("NUKETHEBEES_JOBSERVER_JOB")))
            throw new BenchmarkToolException("Measurement stage requires a jobserver reservation.");
        return JsonSerializer.Deserialize<BenchmarkMeasurementPlan>(File.ReadAllText(path), BenchmarkCommandSupport.JsonOptions)
            ?? throw new BenchmarkToolException("Measurement plan is empty.");
    }
}
