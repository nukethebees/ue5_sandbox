using System.Text.Json;

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
    private const string usage = "Usage: BenchmarkTools <native-simulation|fighter-simulation|frame-memory-level|frame-memory-revision-ab|level-telemetry|gpu-starfield> [options]";

    internal IProcessRunner ProcessRunner => process_runner;
    internal IJobserverLocator JobserverLocator => jobserver_locator;
    internal IEnvironment Environment => environment;
    internal string ExecutablePath => executable_path;
    internal TextWriter StandardOutput => standard_output;
    internal TextWriter StandardError => standard_error;

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
        if (arguments.Count == 0)
        {
            return WriteUsage("A benchmark command is required.");
        }
        if (arguments.Count == 2 && arguments[1] is "--help" or "-h")
        {
            standard_output.WriteLine(usage);
            return 0;
        }
        try
        {
            var repository_paths = RepositoryPaths.Find(working_directory);
            return arguments[0] switch
            {
                "native-simulation" => await RunNativeSimulationCommandAsync(repository_paths, arguments.Skip(1).ToArray(), cancellation_token),
                "fighter-simulation" => await FighterSimulationBenchmarkCommand.RunAsync(this, repository_paths, arguments.Skip(1).ToArray(), cancellation_token),
                "frame-memory-level" => await FrameMemoryLevelBenchmarkCommand.RunAsync(this, repository_paths, arguments.Skip(1).ToArray(), cancellation_token),
                "frame-memory-revision-ab" => await FrameMemoryRevisionAbBenchmarkCommand.RunAsync(this, repository_paths, arguments.Skip(1).ToArray(), cancellation_token),
                "level-telemetry" => await LevelTelemetryBenchmarkCommand.RunAsync(this, repository_paths, arguments.Skip(1).ToArray(), cancellation_token),
                "gpu-starfield" => await GpuStarfieldBenchmarkCommand.RunAsync(this, repository_paths, arguments.Skip(1).ToArray(), cancellation_token),
                _ => WriteUsage($"Unknown benchmark command '{arguments[0]}'."),
            };
        }
        catch (Exception exception) when (exception is BenchmarkToolException or ProcessLaunchException or ProcessTimeoutException or IOException or UnauthorizedAccessException or JsonException)
        {
            standard_error.WriteLine($"BenchmarkTools: {exception.Message}");
            return 1;
        }
    }

    private async Task<int> RunNativeSimulationCommandAsync(
        RepositoryPaths repository_paths,
        IReadOnlyList<string> arguments,
        CancellationToken cancellation_token)
    {
        if (!NativeSimulationBenchmarkCommand.TryParse(arguments, out var request, out var error))
        {
            return WriteUsage(error);
        }

        return await RunNativeSimulationAsync(repository_paths, request!, cancellation_token);
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
            var jobserver_request = JobserverExecution.CreateNativeSimulationRequest(
                jobserver_locator.Locate(),
                executable_path,
                repository_paths,
                request);
            var jobserver_result = await process_runner.RunAsync(jobserver_request, cancellation_token);
            WriteProcessOutput(jobserver_result);
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
        WriteProcessOutput(benchmark_result);
        return benchmark_result.ExitCode;
    }

    private int WriteUsage(string error)
    {
        standard_error.WriteLine(error);
        standard_error.WriteLine(usage);
        return 2;
    }

    internal void WriteProcessOutput(ProcessResult result)
    {
        if (!string.IsNullOrEmpty(result.StandardOutput))
        {
            standard_output.Write(result.StandardOutput);
        }
        if (!string.IsNullOrEmpty(result.StandardError))
        {
            standard_error.Write(result.StandardError);
        }
    }
}
