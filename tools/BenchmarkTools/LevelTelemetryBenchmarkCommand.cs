namespace BenchmarkTools;

internal static class LevelTelemetryBenchmarkCommand
{
    private static readonly HashSet<string> value_arguments = ["--samples", "--output-dir"];

    public static async Task<int> RunAsync(BenchmarkToolsApplication application, RepositoryPaths repository_paths, IReadOnlyList<string> arguments, CancellationToken cancellation_token)
    {
        var parsed = CommandArguments.Parse(arguments, value_arguments, new HashSet<string>(StringComparer.Ordinal));
        var samples = parsed.PositiveInt32("--samples", 7, 100);
        var output = BenchmarkCommandSupport.ResolveOutputDirectory(
            repository_paths,
            parsed.Value("--output-dir", ".local/benchmarks/level-telemetry"));

        Directory.CreateDirectory(output);
        var configure = await application.ProcessRunner.RunAsync(
            JobserverExecution.SetupRequest(application, new ProcessRequest("cmake", ["--preset", "telemetry-benchmark", $"-DIOJ_TELEMETRY_BENCHMARK_SAMPLES={samples}"], repository_paths.Root)), cancellation_token);
        application.WriteProcessOutput(configure);
        if (configure.ExitCode != 0)
        {
            return configure.ExitCode;
        }

        var build = await application.ProcessRunner.RunAsync(
            JobserverExecution.SetupRequest(application, new ProcessRequest("cmake", ["--build", "--preset", "telemetry-benchmark"], repository_paths.Root)), cancellation_token);
        application.WriteProcessOutput(build);
        if (build.ExitCode != 0)
        {
            return build.ExitCode;
        }

        // The CTest benchmark entry acquires machine/exclusive at the measurement boundary.
        var test = await application.ProcessRunner.RunAsync(
            new ProcessRequest("ctest", ["--preset", "telemetry-benchmark", "--output-on-failure", "--verbose"], repository_paths.Root), cancellation_token);
        File.WriteAllText(Path.Combine(output, "telemetry-benchmark.log"), test.StandardOutput + test.StandardError);
        application.WriteProcessOutput(test);
        return test.ExitCode;
    }
}
