using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace BenchmarkTools.Tests;

[TestClass]
public sealed class BenchmarkToolsApplicationTests
{
    [TestMethod]
    public async Task RunAsync_constructs_configure_build_and_benchmark_commands()
    {
        using var repository = new TemporaryRepository();
        var process_runner = new FakeProcessRunner(0, 0, 17);
        var application = CreateApplication(process_runner, @"C:\tools with spaces\BenchmarkTools.exe");

        var exit_code = await application.RunAsync(
            ["native-simulation", "--level", repository.LevelPath, "--seconds", "1.25", "--telemetry", "--fighter-stress-caps", "1000,2000", "--warmup-seconds", "5.5", "--saturation-timeout-seconds", "60.25", "--build-preset", "preset with spaces"],
            repository.Root);

        Assert.AreEqual(17, exit_code);
        Assert.AreEqual(3, process_runner.Requests.Count);
        AssertSetup(process_runner.Requests[0], ["--preset", "preset with spaces"], repository.Root);
        AssertSetup(process_runner.Requests[1], ["--build", "--preset", "preset with spaces"], repository.Root);
        AssertProcess(
            process_runner.Requests[2],
            repository.BenchmarkExecutablePath("preset with spaces"),
            ["--level", repository.LevelPath, "--seconds", "1.25", "--game-speed", "1", "--telemetry", "--fighter-stress-caps", "1000", "2000", "--warmup-seconds", "5.5", "--saturation-timeout-seconds", "60.25"],
            repository.Root);
    }

    [TestMethod]
    public async Task RunAsync_skips_configure_and_build_when_requested()
    {
        using var repository = new TemporaryRepository();
        var process_runner = new FakeProcessRunner(0);
        var application = CreateApplication(process_runner);

        var exit_code = await application.RunAsync(
            ["native-simulation", "--level", repository.LevelPath, "--seconds", "1", "--skip-build"],
            repository.Root);

        Assert.AreEqual(0, exit_code);
        Assert.AreEqual(1, process_runner.Requests.Count);
        Assert.AreEqual(repository.BenchmarkExecutablePath("native-benchmark"), process_runner.Requests[0].FileName);
    }

    [DataTestMethod]
    [DataRow("native-simulation-benchmark", "native-benchmark")]
    [DataRow("frame-memory-level-benchmark", "native-benchmark")]
    [DataRow("custom preset", "custom preset")]
    public async Task RunAsync_runs_the_selected_benchmark_directly(string build_preset, string configure_preset)
    {
        using var repository = new TemporaryRepository();
        repository.CreateBenchmarkExecutable(configure_preset);
        var process_runner = new FakeProcessRunner(23);
        var application = CreateApplication(process_runner);

        var exit_code = await application.RunAsync(
            ["native-simulation", "--level", repository.LevelPath, "--seconds", "1", "--build-preset", build_preset, "--skip-build"],
            repository.Root);

        Assert.AreEqual(23, exit_code);
        Assert.AreEqual(1, process_runner.Requests.Count);
        AssertProcess(
            process_runner.Requests[0],
            repository.BenchmarkExecutablePath(configure_preset),
            ["--level", repository.LevelPath, "--seconds", "1", "--game-speed", "1"],
            repository.Root);
    }

    [DataTestMethod]
    [DataRow("native-simulation-benchmark")]
    [DataRow("frame-memory-level-benchmark")]
    public async Task RunAsync_uses_the_shared_native_configuration_for_benchmark_builds(string build_preset)
    {
        using var repository = new TemporaryRepository();
        var process_runner = new FakeProcessRunner(0, 0, 0);
        var application = CreateApplication(process_runner);

        var exit_code = await application.RunAsync(
            ["native-simulation", "--level", repository.LevelPath, "--seconds", "1", "--build-preset", build_preset],
            repository.Root);

        Assert.AreEqual(0, exit_code);
        Assert.AreEqual(3, process_runner.Requests.Count);
        AssertSetup(process_runner.Requests[0], ["--preset", "native-benchmark"], repository.Root);
        AssertSetup(process_runner.Requests[1], ["--build", "--preset", build_preset], repository.Root);
    }

    [DataTestMethod]
    [DataRow(0)]
    [DataRow(1)]
    public async Task RunAsync_reports_configure_and_build_failures_without_starting_a_benchmark(int failed_step)
    {
        using var repository = new TemporaryRepository();
        var process_runner = new BuildFailureRunner(failed_step);
        var standard_output = new StringWriter();
        var standard_error = new StringWriter();
        var application = new BenchmarkToolsApplication(process_runner, standard_output, standard_error, @"C:\tools\BenchmarkTools.exe");

        var exit_code = await application.RunAsync(
            ["native-simulation", "--level", repository.LevelPath, "--seconds", "1"], repository.Root);

        Assert.AreEqual(19, exit_code);
        Assert.AreEqual(failed_step + 1, process_runner.Calls);
        StringAssert.Contains(standard_output.ToString(), "build progress");
        StringAssert.Contains(standard_error.ToString(), "CMake diagnostic");
    }

    [TestMethod]
    public async Task RunAsync_returns_a_parse_error_without_launching_a_process()
    {
        using var repository = new TemporaryRepository();
        var process_runner = new FakeProcessRunner();
        var standard_error = new StringWriter();
        var application = CreateApplication(process_runner, standard_error: standard_error);

        var exit_code = await application.RunAsync(
            ["native-simulation", "--level", repository.LevelPath, "--seconds", "1", "--fighter-stress-cap", "1", "--fighter-stress-caps", "2"],
            repository.Root);

        Assert.AreEqual(2, exit_code);
        Assert.AreEqual(0, process_runner.Requests.Count);
        StringAssert.Contains(standard_error.ToString(), "mutually exclusive");
    }

    private static BenchmarkToolsApplication CreateApplication(
        FakeProcessRunner process_runner,
        string executable_path = @"C:\tools\BenchmarkTools.exe",
        TextWriter? standard_error = null)
    {
        return new BenchmarkToolsApplication(
            process_runner,
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

    private static void AssertSetup(ProcessRequest request, string[] arguments, string root)
    {
        AssertProcess(request, "cmake", arguments, root);
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

    private sealed class BuildFailureRunner(int failed_step) : IProcessRunner
    {
        public int Calls { get; private set; }

        public Task<ProcessResult> RunAsync(ProcessRequest request, CancellationToken cancellation_token)
        {
            return Task.FromResult(Calls++ == failed_step
                ? new ProcessResult(19, "build progress", "CMake diagnostic")
                : new ProcessResult(0));
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
            CreateBenchmarkExecutable("native-benchmark");
            CreateBenchmarkExecutable("preset with spaces");
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
