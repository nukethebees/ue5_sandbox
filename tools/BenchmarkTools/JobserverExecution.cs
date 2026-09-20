namespace BenchmarkTools;

internal interface IJobserverLocator
{
    string Locate();
}

internal sealed class JobserverLocator : IJobserverLocator
{
    public string Locate()
    {
        var local_application_data = Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData);
        if (string.IsNullOrWhiteSpace(local_application_data))
        {
            throw new BenchmarkToolException("LOCALAPPDATA is required to locate the per-user jobserver.");
        }

        var jobserver_path = Path.Combine(
            local_application_data,
            "NukeTheBees",
            "jobserver",
            "bin",
            "jobserver.exe");
        if (!File.Exists(jobserver_path))
        {
            throw new BenchmarkToolException($"The jobserver executable was not found: '{jobserver_path}'. Run csetup to install it.");
        }

        return jobserver_path;
    }
}

internal static class JobserverExecution
{
    public static ProcessRequest CreateRequest(
        string jobserver_path,
        string benchmark_tools_path,
        RepositoryPaths repository_paths,
        string name,
        IReadOnlyList<string> command_arguments,
        IReadOnlyList<string>? shared_resources = null)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(jobserver_path);
        ArgumentException.ThrowIfNullOrWhiteSpace(benchmark_tools_path);
        ArgumentNullException.ThrowIfNull(repository_paths);
        ArgumentException.ThrowIfNullOrWhiteSpace(name);
        ArgumentNullException.ThrowIfNull(command_arguments);

        var arguments = new List<string>
        {
            "run",
            "--name",
            name,
            "--kind",
            "benchmark",
            "--worktree",
            repository_paths.Root,
            "--exclusive",
            "machine",
            "--exclusive",
            "benchmark",
        };
        if (shared_resources is not null)
        {
            foreach (var resource in shared_resources)
            {
                arguments.Add("--shared");
                arguments.Add(resource);
            }
        }
        arguments.Add("--");
        arguments.Add(benchmark_tools_path);
        arguments.AddRange(command_arguments);
        return new ProcessRequest(jobserver_path, arguments, repository_paths.Root);
    }

    public static ProcessRequest CreateNativeSimulationRequest(
        string jobserver_path,
        string benchmark_tools_path,
        RepositoryPaths repository_paths,
        NativeSimulationBenchmarkRequest request)
    {
        return CreateRequest(
            jobserver_path,
            benchmark_tools_path,
            repository_paths,
            "native simulation benchmark",
            request.ToCommandArguments(include_skip_build: true));
    }
}
