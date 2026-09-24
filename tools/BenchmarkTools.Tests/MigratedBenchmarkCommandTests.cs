using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace BenchmarkTools.Tests;

[TestClass]
public sealed class MigratedBenchmarkCommandTests
{
    [TestMethod]
    public async Task FrameMemoryLevel_emits_a_validated_native_result()
    {
        using var repository = new TemporaryRepository();
        var output = new StringWriter();
        var runner = new RecordingRunner(_ => new ProcessResult(0, "{\"level\":{\"id\":\"batch-benchmark\"},\"workload\":{\"completed_ticks\":1,\"requested_ticks\":1,\"game_speed\":100,\"advance_calls\":1},\"memory\":{\"frame_overflow_count\":0,\"frame_peak_claimed_bytes\":1},\"final_state\":{\"peak_fighters\":1}}\n"));
        var application = CreateApplication(runner, output);

        var exit_code = await application.RunAsync(["frame-memory-level", "--skip-build"], repository.Root);

        Assert.AreEqual(0, exit_code);
        Assert.AreEqual("native-simulation", runner.Requests.Single().Arguments[0]);
        StringAssert.Contains(output.ToString(), "batch-benchmark");
    }

    [TestMethod]
    public void EngineResource_matches_the_cmake_identity_shape()
    {
        var resource = BenchmarkCommandSupport.EngineResource(@"C:\Unreal Engine\Engine\Binaries\Win64\UnrealEditor-Cmd.exe");

        StringAssert.StartsWith(resource, "unreal-build/");
        Assert.AreEqual("unreal-build/".Length + 64, resource.Length);
    }

    [TestMethod]
    public async Task MigratedCommands_reject_invalid_options_before_launching_processes()
    {
        using var repository = new TemporaryRepository();
        var runner = new RecordingRunner(_ => throw new AssertFailedException("The command should not launch a process."));
        var errors = new StringWriter();
        var application = new BenchmarkToolsApplication(runner, new FakeJobserverLocator(), new FakeEnvironment(), TextWriter.Null, errors, @"C:\tools\BenchmarkTools.exe");

        Assert.AreEqual(1, await application.RunAsync(["fighter-simulation", "--fighter-caps", "1,1"], repository.Root));
        Assert.AreEqual(1, await application.RunAsync(["level-telemetry", "--samples", "0"], repository.Root));
        Assert.AreEqual(1, await application.RunAsync(["gpu-starfield", "--editor", "editor.exe", "--project", "project.uproject", "--output", "output", "--counts", "0"], repository.Root));
        Assert.AreEqual(1, await application.RunAsync(["frame-memory-revision-ab", "--skip-build"], repository.Root));
        StringAssert.Contains(errors.ToString(), "fighter-caps");
        StringAssert.Contains(errors.ToString(), "skip-build");
    }

    [TestMethod]
    public async Task MigratedCommands_report_malformed_structured_results_cleanly()
    {
        using var repository = new TemporaryRepository();
        var errors = new StringWriter();
        var runner = new RecordingRunner(request =>
        {
            if (request.FileName == "git")
            {
                return new ProcessResult(0, "commit\n");
            }
            return new ProcessResult(0, "{}\n");
        });
        var application = new BenchmarkToolsApplication(runner, new FakeJobserverLocator(), new FakeEnvironment(), TextWriter.Null, errors, @"C:\tools\BenchmarkTools.exe");
        var prepared = Path.Combine(repository.Root, "prepared");
        Directory.CreateDirectory(prepared);

        Assert.AreEqual(1, await application.RunAsync(["frame-memory-level", "--skip-build"], repository.Root));
        Assert.AreEqual(1, await application.RunAsync(["fighter-simulation", "--fighter-caps", "1", "--skip-build"], repository.Root));
        Assert.AreEqual(1, await application.RunAsync(["frame-memory-revision-ab", "--skip-build", "--baseline-worktree", prepared], repository.Root));
        StringAssert.Contains(errors.ToString(), "malformed");
    }

    [TestMethod]
    public async Task FrameMemoryRevisionAb_prepare_only_removes_its_managed_worktree()
    {
        using var repository = new TemporaryRepository();
        var runner = new RecordingRunner(request =>
        {
            if (request.FileName == "git" && request.Arguments.Contains("rev-parse"))
            {
                return new ProcessResult(0, "commit\n");
            }
            if (request.FileName == "git" && request.Arguments.Contains("worktree") && request.Arguments.Contains("add"))
            {
                var detached = request.Arguments.ToList().IndexOf("--detach");
                Directory.CreateDirectory(request.Arguments[detached + 1]);
            }
            return new ProcessResult(0);
        });
        var application = CreateApplication(runner, TextWriter.Null);

        var exit_code = await application.RunAsync(["frame-memory-revision-ab", "--prepare-only"], repository.Root);

        Assert.AreEqual(0, exit_code);
        Assert.IsTrue(runner.Requests.Any(request => request.Arguments.Contains("worktree") && request.Arguments.Contains("remove") && request.Arguments.Contains("--force")));
        Assert.IsTrue(runner.Requests.Where(request => request.FileName == "git").All(request => request.Arguments.Contains("core.hooksPath=NUL")));
    }

    private static BenchmarkToolsApplication CreateApplication(RecordingRunner runner, TextWriter output)
    {
        return new BenchmarkToolsApplication(runner, new FakeJobserverLocator(), new FakeEnvironment(), output, TextWriter.Null, @"C:\tools\BenchmarkTools.exe");
    }

    private sealed class RecordingRunner(Func<ProcessRequest, ProcessResult> result_factory) : IProcessRunner
    {
        public List<ProcessRequest> Requests { get; } = [];

        public Task<ProcessResult> RunAsync(ProcessRequest request, CancellationToken cancellation_token)
        {
            Requests.Add(request);
            return Task.FromResult(result_factory(request));
        }
    }

    private sealed class FakeJobserverLocator : IJobserverLocator
    {
        public string Locate() => @"C:\jobserver\jobserver.exe";
    }

    private sealed class FakeEnvironment : IEnvironment
    {
        public string? GetEnvironmentVariable(string variable_name) => "job";
    }

    private sealed class TemporaryRepository : IDisposable
    {
        public TemporaryRepository()
        {
            Root = Path.Combine(Path.GetTempPath(), $"BenchmarkToolsMigration{Guid.NewGuid():N}");
            Directory.CreateDirectory(Root);
            Directory.CreateDirectory(Path.Combine(Root, ".git"));
            File.WriteAllText(Path.Combine(Root, "CMakeLists.txt"), string.Empty);
        }

        public string Root { get; }

        public void Dispose() => Directory.Delete(Root, recursive: true);
    }
}
