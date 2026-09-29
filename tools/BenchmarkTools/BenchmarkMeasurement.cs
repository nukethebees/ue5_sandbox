using System.Text.Json;

namespace BenchmarkTools;

// The parent owns preparation, analysis and cleanup; the child executes the complete measurement plan.
internal sealed record BenchmarkMeasurementPlan(string Output, RevisionIdentity Candidate, RevisionIdentity Baseline,
    IReadOnlyList<BenchmarkRepetition> Sequence, SandboxIsmcRequest? Ismc = null, bool ValidationOnly = false);

internal static class BenchmarkMeasurement
{
    public static async Task RunAsync(BenchmarkToolsApplication application, RepositoryPaths repository, string command,
        BenchmarkMeasurementPlan plan, CancellationToken token)
    {
        await VerifySourcesAsync(application, plan, token);
        var path = Path.Combine(plan.Output, "measurement-plan.json");
        BenchmarkCommandSupport.WriteJson(path, plan);
        var request = new ProcessRequest(application.ExecutablePath, [command, "--measurement-plan", path], repository.Root);
        var result = await application.ProcessRunner.RunAsync(request, token);
        application.WriteProcessOutput(result);
        if (result.ExitCode != 0) throw new BenchmarkToolException($"Measurement process failed with exit code {result.ExitCode}; see '{plan.Output}'.");
        await VerifySourcesAsync(application, plan, token);
    }

    public static async Task<BenchmarkMeasurementPlan> ReadAsync(BenchmarkToolsApplication application, string path, CancellationToken token)
    {
        var plan = JsonSerializer.Deserialize<BenchmarkMeasurementPlan>(File.ReadAllText(path), BenchmarkCommandSupport.JsonOptions)
            ?? throw new BenchmarkToolException("Measurement plan is empty.");
        await VerifySourcesAsync(application, plan, token);
        return plan;
    }

    private static async Task VerifySourcesAsync(BenchmarkToolsApplication application, BenchmarkMeasurementPlan plan, CancellationToken token)
    {
        await BenchmarkRunContext.VerifySourceAsync(application, plan.Candidate, token);
        if (plan.Baseline.Root != plan.Candidate.Root)
            await BenchmarkRunContext.VerifySourceAsync(application, plan.Baseline, token);
    }
}
