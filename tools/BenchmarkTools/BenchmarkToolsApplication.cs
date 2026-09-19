namespace BenchmarkTools;

internal interface IEnvironment
{
    string? GetEnvironmentVariable(string variable_name);
}

internal sealed class ProcessEnvironment : IEnvironment
{
    public string? GetEnvironmentVariable(string variable_name)
    {
        return Environment.GetEnvironmentVariable(variable_name);
    }
}

internal sealed class BenchmarkToolException(string message) : Exception(message);

internal sealed class BenchmarkToolsApplication(
    IProcessRunner process_runner,
    IJobserverLocator jobserver_locator,
    IEnvironment environment,
    TextWriter standard_output,
    TextWriter standard_error,
    string executable_path)
{
    private const string usage = "Usage: BenchmarkTools native-simulation --level <path> --seconds <value> [--game-speed <value>] [--telemetry] [--fighter-stress-cap <value> | --fighter-stress-caps <a,b,c>] [--warmup-seconds <value>] [--saturation-timeout-seconds <value>] [--build-preset <preset>] [--skip-build]";

    public async Task<int> RunAsync(
        IReadOnlyList<string> arguments,
        string working_directory,
        CancellationToken cancellation_token = default)
    {
        ArgumentNullException.ThrowIfNull(arguments);

        if (arguments.Count == 1 && arguments[0] is "--help" or "-h")
        {
            standard_output.WriteLine(usage);
            return 0;
        }
        if (arguments.Count == 0 || !string.Equals(arguments[0], "native-simulation", StringComparison.Ordinal))
        {
            return WriteUsage("Expected the 'native-simulation' command.");
        }
        if (arguments.Count == 2 && arguments[1] is "--help" or "-h")
        {
            standard_output.WriteLine(usage);
            return 0;
        }
        if (!NativeSimulationBenchmarkCommand.TryParse(arguments.Skip(1).ToArray(), out var request, out var error))
        {
            return WriteUsage(error);
        }

        try
        {
            var repository_paths = RepositoryPaths.Find(working_directory);
            return await RunNativeSimulationAsync(repository_paths, request!, cancellation_token);
        }
        catch (Exception exception) when (exception is BenchmarkToolException or ProcessLaunchException or IOException or UnauthorizedAccessException)
        {
            standard_error.WriteLine($"BenchmarkTools: {exception.Message}");
            return 1;
        }
    }

    private async Task<int> RunNativeSimulationAsync(
        RepositoryPaths repository_paths,
        NativeSimulationBenchmarkRequest request,
        CancellationToken cancellation_token)
    {
        if (!File.Exists(request.LevelPath))
        {
            throw new BenchmarkToolException($"The level path does not exist: '{request.LevelPath}'.");
        }

        if (!request.SkipBuild)
        {
            var configure_result = await process_runner.RunAsync(
                new ProcessRequest("cmake", ["--preset", request.BuildPreset], repository_paths.Root),
                cancellation_token);
            if (configure_result.ExitCode != 0)
            {
                return configure_result.ExitCode;
            }

            var build_result = await process_runner.RunAsync(
                new ProcessRequest("cmake", ["--build", "--preset", request.BuildPreset], repository_paths.Root),
                cancellation_token);
            if (build_result.ExitCode != 0)
            {
                return build_result.ExitCode;
            }
        }

        if (string.IsNullOrWhiteSpace(environment.GetEnvironmentVariable("NUKETHEBEES_JOBSERVER_JOB")))
        {
            var jobserver_request = JobserverExecution.CreateRequest(
                jobserver_locator.Locate(),
                executable_path,
                repository_paths,
                request);
            var jobserver_result = await process_runner.RunAsync(jobserver_request, cancellation_token);
            return jobserver_result.ExitCode;
        }

        var benchmark_path = repository_paths.BenchmarkExecutable(request.BuildPreset);
        if (!File.Exists(benchmark_path))
        {
            throw new BenchmarkToolException($"The native simulation benchmark executable was not built: '{benchmark_path}'.");
        }

        var benchmark_result = await process_runner.RunAsync(
            new ProcessRequest(benchmark_path, request.ToBenchmarkArguments(), repository_paths.Root),
            cancellation_token);
        return benchmark_result.ExitCode;
    }

    private int WriteUsage(string error)
    {
        standard_error.WriteLine(error);
        standard_error.WriteLine(usage);
        return 2;
    }
}
