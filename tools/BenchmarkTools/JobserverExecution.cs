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
        NativeSimulationBenchmarkRequest request)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(jobserver_path);
        ArgumentException.ThrowIfNullOrWhiteSpace(benchmark_tools_path);
        ArgumentNullException.ThrowIfNull(repository_paths);
        ArgumentNullException.ThrowIfNull(request);

        var arguments = new List<string>
        {
            "run",
            "--name",
            "native simulation benchmark",
            "--kind",
            "benchmark",
            "--worktree",
            repository_paths.Root,
            "--exclusive",
            "machine",
            "--exclusive",
            "benchmark",
            "--",
            benchmark_tools_path,
        };
        arguments.AddRange(request.ToCommandArguments(include_skip_build: true));
        return new ProcessRequest(jobserver_path, arguments, repository_paths.Root);
    }
}
