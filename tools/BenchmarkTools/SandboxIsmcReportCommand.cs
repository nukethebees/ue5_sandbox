using System.Text.Json;

namespace BenchmarkTools;

internal static class SandboxIsmcReportCommand
{
    private static readonly JsonSerializerOptions read_options = new(BenchmarkCommandSupport.JsonOptions)
    {
        RespectNullableAnnotations = true, RespectRequiredConstructorParameters = true,
    };
    public static int Run(BenchmarkToolsApplication application, IReadOnlyList<string> arguments, string working_directory)
    {
        var parsed = CommandArguments.Parse(arguments, new HashSet<string> { "--run-dir" }, new HashSet<string>());
        var directory = Path.GetFullPath(parsed.Required("--run-dir"), working_directory);
        var manifest = Read<BenchmarkManifest>(directory, "manifest.json");
        if (manifest.SchemaVersion != 1 || manifest.Benchmark != "sandbox-ismc-revision-ab")
            throw new BenchmarkToolException("Run directory is not a supported SandboxISMC revision comparison.");
        if (manifest.Status is not ("complete" or "incomparable"))
            throw new BenchmarkToolException($"Cannot report an incomplete or failed comparison (status: {manifest.Status}).");
        var plan = Read<BenchmarkMeasurementPlan>(directory, "measurement-plan.json");
        var sequence = Read<List<BenchmarkRepetition>>(directory, "sequence.json");
        if (!plan.Sequence.SequenceEqual(sequence)) throw new BenchmarkToolException("Comparison plan and sequence disagree.");
        var captures = Read<List<SandboxIsmcCapture>>(directory, "captures.json");
        var comparison = SandboxIsmcResults.GenerateReports(directory, manifest, plan, captures);
        application.StandardOutput.WriteLine($"Reports regenerated: {directory}");
        return comparison.Comparable ? 0 : 1;
    }

    internal static T Read<T>(string directory, string name) => JsonSerializer.Deserialize<T>(File.ReadAllText(Path.Combine(directory, name)), read_options)
        ?? throw new BenchmarkToolException($"Empty {name}.");
}
