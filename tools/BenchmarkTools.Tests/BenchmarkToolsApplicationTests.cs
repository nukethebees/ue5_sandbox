using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace BenchmarkTools.Tests;

[TestClass]
public sealed class BenchmarkToolsApplicationTests
{
    [TestMethod]
    public async Task RunAsync_constructs_configure_build_and_jobserver_commands()
    {
        using var repository = new TemporaryRepository();
        var process_runner = new FakeProcessRunner(0, 0, 17);
        var application = CreateApplication(process_runner, new FakeEnvironment(), @"C:\tools with spaces\BenchmarkTools.exe");

        var exit_code = await application.RunAsync(
            ["native-simulation", "--level", repository.LevelPath, "--seconds", "1.25", "--telemetry", "--fighter-stress-caps", "1000,2000", "--warmup-seconds", "5.5", "--saturation-timeout-seconds", "60.25", "--build-preset", "preset with spaces"],
            repository.Root);

        Assert.AreEqual(17, exit_code);
        Assert.AreEqual(3, process_runner.Requests.Count);
        AssertProcess(process_runner.Requests[0], "cmake", ["--preset", "preset with spaces"], repository.Root);
        AssertProcess(process_runner.Requests[1], "cmake", ["--build", "--preset", "preset with spaces"], repository.Root);
        AssertProcess(
            process_runner.Requests[2],
            @"C:\jobserver\jobserver.exe",
            ["run", "--name", "native simulation benchmark", "--kind", "benchmark", "--worktree", repository.Root, "--exclusive", "machine", "--exclusive", "benchmark", "--", @"C:\tools with spaces\BenchmarkTools.exe", "native-simulation", "--level", repository.LevelPath, "--seconds", "1.25", "--game-speed", "1", "--build-preset", "preset with spaces", "--telemetry", "--fighter-stress-caps", "1000,2000", "--warmup-seconds", "5.5", "--saturation-timeout-seconds", "60.25", "--skip-build"],
            repository.Root);
    }

    [TestMethod]
    public async Task RunAsync_skips_configure_and_build_when_requested()
    {
        using var repository = new TemporaryRepository();
        var process_runner = new FakeProcessRunner(0);
        var application = CreateApplication(process_runner, new FakeEnvironment());

        var exit_code = await application.RunAsync(
            ["native-simulation", "--level", repository.LevelPath, "--seconds", "1", "--skip-build"],
            repository.Root);

        Assert.AreEqual(0, exit_code);
        Assert.AreEqual(1, process_runner.Requests.Count);
        Assert.AreEqual(@"C:\jobserver\jobserver.exe", process_runner.Requests[0].FileName);
    }

    [TestMethod]
    public async Task RunAsync_reentry_runs_benchmark_without_reacquiring_the_jobserver()
    {
        using var repository = new TemporaryRepository();
        repository.CreateBenchmarkExecutable("native-simulation-benchmark");
        var process_runner = new FakeProcessRunner(23);
        var application = CreateApplication(process_runner, new FakeEnvironment("job-123"));

        var exit_code = await application.RunAsync(
            ["native-simulation", "--level", repository.LevelPath, "--seconds", "1", "--skip-build"],
            repository.Root);

        Assert.AreEqual(23, exit_code);
        Assert.AreEqual(1, process_runner.Requests.Count);
        AssertProcess(
            process_runner.Requests[0],
            repository.BenchmarkExecutablePath("native-simulation-benchmark"),
            ["--level", repository.LevelPath, "--seconds", "1", "--game-speed", "1"],
            repository.Root);
    }

    [TestMethod]
    public async Task RunAsync_returns_a_parse_error_without_launching_a_process()
    {
        using var repository = new TemporaryRepository();
        var process_runner = new FakeProcessRunner();
        var standard_error = new StringWriter();
        var application = CreateApplication(process_runner, new FakeEnvironment(), standard_error: standard_error);

        var exit_code = await application.RunAsync(
            ["native-simulation", "--level", repository.LevelPath, "--seconds", "1", "--fighter-stress-cap", "1", "--fighter-stress-caps", "2"],
            repository.Root);

        Assert.AreEqual(2, exit_code);
        Assert.AreEqual(0, process_runner.Requests.Count);
        StringAssert.Contains(standard_error.ToString(), "mutually exclusive");
    }

    private static BenchmarkToolsApplication CreateApplication(
        FakeProcessRunner process_runner,
        FakeEnvironment environment,
        string executable_path = @"C:\tools\BenchmarkTools.exe",
        TextWriter? standard_error = null)
    {
        return new BenchmarkToolsApplication(
            process_runner,
            new FakeJobserverLocator(),
            environment,
            TextWriter.Null,
            standard_error ?? TextWriter.Null,
            executable_path);
    }

    private static void AssertProcess(ProcessRequest request, string file_name, string[] arguments, string working_directory)
    {
        Assert.AreEqual(file_name, request.FileName);
        Assert.AreEqual(working_directory, request.WorkingDirectory);
        CollectionAssert.AreEqual(arguments, request.Arguments.ToArray());
    }

    private sealed class FakeProcessRunner(params int[] exit_codes) : IProcessRunner
    {
        private readonly Queue<int> exit_codes_ = new(exit_codes);

        public List<ProcessRequest> Requests { get; } = [];

        public Task<ProcessResult> RunAsync(ProcessRequest request, CancellationToken cancellation_token)
        {
            Requests.Add(request);
            return Task.FromResult(new ProcessResult(exit_codes_.Count > 0 ? exit_codes_.Dequeue() : 0));
        }
    }

    private sealed class FakeJobserverLocator : IJobserverLocator
    {
        public string Locate() => @"C:\jobserver\jobserver.exe";
    }

    private sealed class FakeEnvironment(string? job_id = null) : IEnvironment
    {
        public string? GetEnvironmentVariable(string variable_name)
        {
            return variable_name == "NUKETHEBEES_JOBSERVER_JOB" ? job_id : null;
        }
    }

    private sealed class TemporaryRepository : IDisposable
    {
        public TemporaryRepository()
        {
            Root = Path.Combine(Path.GetTempPath(), $"Benchmark Tools Tests {Guid.NewGuid():N}");
            Directory.CreateDirectory(Root);
            Directory.CreateDirectory(Path.Combine(Root, ".git"));
            File.WriteAllText(Path.Combine(Root, "CMakeLists.txt"), string.Empty);
            LevelPath = Path.Combine(Root, "Level Scripts", "benchmark level.scm");
            Directory.CreateDirectory(Path.GetDirectoryName(LevelPath)!);
            File.WriteAllText(LevelPath, "(level)");
        }

        public string Root { get; }

        public string LevelPath { get; }

        public string BenchmarkExecutablePath(string preset)
        {
            return Path.Combine(Root, "out", "build", preset, "bin", "native-simulation-benchmark.exe");
        }

        public void CreateBenchmarkExecutable(string preset)
        {
            var executable_path = BenchmarkExecutablePath(preset);
            Directory.CreateDirectory(Path.GetDirectoryName(executable_path)!);
            File.WriteAllText(executable_path, string.Empty);
        }

        public void Dispose()
        {
            Directory.Delete(Root, recursive: true);
        }
    }
}
