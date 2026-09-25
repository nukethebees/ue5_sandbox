namespace BenchmarkTools;

internal sealed record RepositoryPaths(string Root)
{
    public static RepositoryPaths Find(string starting_directory)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(starting_directory);

        var directory = new DirectoryInfo(Path.GetFullPath(starting_directory));
        while (directory is not null)
        {
            var git_path = Path.Combine(directory.FullName, ".git");
            var cmake_path = Path.Combine(directory.FullName, "CMakeLists.txt");
            if ((Directory.Exists(git_path) || File.Exists(git_path)) && File.Exists(cmake_path))
            {
                return new RepositoryPaths(directory.FullName);
            }

            directory = directory.Parent;
        }

        throw new BenchmarkToolException($"Could not locate the repository root from '{starting_directory}'.");
    }

    public string BenchmarkExecutable(string build_preset)
    {
        return Path.Combine(Root, "out", "build", NativeSimulationConfigurePreset(build_preset), "bin", "native-simulation-benchmark.exe");
    }

    public static string NativeSimulationConfigurePreset(string build_preset) => build_preset switch
    {
        "native-simulation-benchmark" or "frame-memory-level-benchmark" => "native-benchmark",
        _ => build_preset,
    };
}
