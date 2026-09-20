using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace BenchmarkTools.Tests;

[TestClass]
public sealed class GpuStarfieldBenchmarkCommandTests
{
    [TestMethod]
    public void ParseRequest_uses_defaults_and_accepts_explicit_lists()
    {
        var paths = new RepositoryPaths(Path.GetTempPath());
        var defaults = GpuStarfieldBenchmarkCommand.ParseRequest(["--editor", "editor.exe", "--project", "project.uproject", "--output", "results"], paths);
        var explicit_request = GpuStarfieldBenchmarkCommand.ParseRequest([
            "--editor=editor.exe", "--project=project.uproject", "--output=results", "--counts=10,20",
            "--resolutions=1280x720,1920x1080", "--size-multipliers=1,4", "--camera-modes=stationary",
            "--warmup-frames=3", "--capture-frames=9", "--repeats=2", "--trim-frames=1", "--timeout-seconds=20",
        ], paths);

        CollectionAssert.AreEqual(new[] { 10000, 100000, 1000000 }, defaults.Counts.ToArray());
        CollectionAssert.AreEqual(new[] { false, true }, defaults.CameraModes.ToArray());
        Assert.AreEqual(4, explicit_request.Configurations.Count);
        CollectionAssert.AreEqual(new[] { 10, 20 }, explicit_request.Counts.ToArray());
        Assert.AreEqual(3, explicit_request.WarmupFrames);
        Assert.AreEqual(9, explicit_request.CaptureFrames);
        Assert.AreEqual(2, explicit_request.Repeats);
    }

    [DataTestMethod]
    [DataRow("--counts=10,10")]
    [DataRow("--resolutions=not-a-resolution")]
    [DataRow("--size-multipliers=101")]
    [DataRow("--camera-modes=orbiting")]
    public void ParseRequest_rejects_duplicate_or_invalid_lists(string option)
    {
        var paths = new RepositoryPaths(Path.GetTempPath());

        Assert.ThrowsException<BenchmarkToolException>(() => GpuStarfieldBenchmarkCommand.ParseRequest(["--editor=editor.exe", "--project=project.uproject", "--output=results", option], paths));
    }

    [TestMethod]
    public void EditorArguments_preserve_the_expected_Unreal_benchmark_options()
    {
        var request = Request(camera_modes: [false, true]);
        var arguments = GpuStarfieldBenchmarkCommand.EditorArguments(request, request.Configurations.Single(), "C:\\raw output");

        CollectionAssert.Contains(arguments.ToList(), "-GpuStarfieldBenchmarkCounts=100");
        CollectionAssert.Contains(arguments.ToList(), "-GpuStarfieldBenchmarkCameraModes=stationary,moving");
        CollectionAssert.Contains(arguments.ToList(), "-GpuStarfieldBenchmarkOutput=C:\\raw output");
        CollectionAssert.Contains(arguments.ToList(), "-csvGpuStats");
    }

    [TestMethod]
    public void ParseCsv_handles_quoted_fields_and_rejects_unterminated_fields()
    {
        CollectionAssert.AreEqual(new[] { "plain", "quoted,value", "escaped\"quote" }, GpuStarfieldBenchmarkCommand.ParseCsv("plain,\"quoted,value\",\"escaped\"\"quote\"").ToArray());
        Assert.ThrowsException<BenchmarkToolException>(() => GpuStarfieldBenchmarkCommand.ParseCsv("plain,\"unterminated"));
    }

    [TestMethod]
    public void ReadCapture_trims_frames_and_validates_rendered_resolution()
    {
        using var fixture = new CaptureFixture();
        var configuration = new GpuStarfieldBenchmarkCommand.Configuration(1280, 720, 1);
        fixture.Write("tag,width,GameThreadTime,GPUTime\n[systemresolution.resx],1280,1,2\n[systemresolution.resy],720,2,3\nframe,,3,4\nframe,,4,5\nframe,,5,6\n");

        var capture = GpuStarfieldBenchmarkCommand.ReadCapture(fixture.Path, configuration, 100, 1, false, false, 1);

        Assert.AreEqual(5, capture.FrameCount);
        Assert.AreEqual(3.0, capture.Medians["game_thread_ms"]);
        fixture.Write("tag,width,GameThreadTime\n[systemresolution.resx],640,1\n[systemresolution.resy],480,2\nframe,,3\n");
        Assert.ThrowsException<BenchmarkToolException>(() => GpuStarfieldBenchmarkCommand.ReadCapture(fixture.Path, configuration, 100, 1, false, false, 0));
    }

    [TestMethod]
    public void CalculateDeltas_calculates_paired_medians_and_rejects_missing_captures()
    {
        var request = Request(repeats: 2);
        var captures = new List<GpuStarfieldBenchmarkCommand.Capture>();
        foreach (var repeat in Enumerable.Range(1, 2))
        {
            captures.Add(Capture(request.Configurations.Single(), 100, repeat, false, false, 10 + repeat));
            captures.Add(Capture(request.Configurations.Single(), 100, repeat, true, false, 10.5 + repeat));
        }

        var deltas = GpuStarfieldBenchmarkCommand.CalculateDeltas(captures, request);

        Assert.AreEqual(0.5, deltas.Single(delta => delta.Metric == "gpu_ms").DeltaMedian, 0.0001);
        Assert.ThrowsException<BenchmarkToolException>(() => GpuStarfieldBenchmarkCommand.CalculateDeltas(captures.Take(3).ToArray(), request));
    }

    [TestMethod]
    public void Validate_detects_unexpected_draws_and_moving_primitive_disappearance()
    {
        var request = Request(camera_modes: [true]);
        var deltas = Deltas(request, draw_delta: 2, primitive_delta: 202);
        var capture = Capture(request.Configurations.Single(), 100, 1, true, true, 1, primitive_minimum: 200);

        var exception = Assert.ThrowsException<BenchmarkToolException>(() => GpuStarfieldBenchmarkCommand.Validate([capture], deltas, request));

        StringAssert.Contains(exception.Message, "disappeared");
        Assert.ThrowsException<BenchmarkToolException>(() => GpuStarfieldBenchmarkCommand.Validate([], Deltas(request, draw_delta: 1, primitive_delta: 202), request));
    }

    [TestMethod]
    public void RenderMarkdown_includes_translucency_delta_ranges_and_moving_note()
    {
        var request = Request(camera_modes: [true]);
        var markdown = GpuStarfieldBenchmarkCommand.RenderMarkdown(Deltas(request, draw_delta: 2, primitive_delta: 202), request);

        StringAssert.Contains(markdown, "Translucency GPU delta ms");
        StringAssert.Contains(markdown, "Paired delta ranges:");
        StringAssert.Contains(markdown, "`translucency_gpu_ms`");
        StringAssert.Contains(markdown, "deterministic curved path");
    }

    [TestMethod]
    public void WriteOutputs_emits_structured_json_and_csv()
    {
        using var repository = new TestRepository();
        var request = Request();
        var captures = new[] { Capture(request.Configurations.Single(), 100, 1, false, false, 1) };

        GpuStarfieldBenchmarkCommand.WriteOutputs(repository.Root, request, captures, Deltas(request, draw_delta: 2, primitive_delta: 202));

        StringAssert.Contains(File.ReadAllText(Path.Combine(repository.Root, "GpuStarfieldBenchmark.json")), "\"schemaVersion\": 1");
        StringAssert.Contains(File.ReadAllText(Path.Combine(repository.Root, "GpuStarfieldBenchmark.csv")), "translucency_gpu_ms");
    }

    [TestMethod]
    public async Task Cmake_style_gpu_arguments_parse_through_the_application()
    {
        using var repository = new TestRepository();
        var editor = repository.CreateFile(Path.Combine("Engine", "Binaries", "Win64", "UnrealEditor-Cmd.exe"));
        var project = repository.CreateFile("Sandbox.uproject");
        var runner = new RecordingRunner();
        var application = new BenchmarkToolsApplication(runner, new FakeJobserverLocator(), new EmptyEnvironment(), TextWriter.Null, TextWriter.Null, "BenchmarkTools.exe");

        var exit_code = await application.RunAsync([
            "gpu-starfield", $"--editor={editor}", $"--project={project}", $"--output={Path.Combine(repository.Root, "results")}",
            "--counts=100000,1000000", "--resolutions=1920x1080,3840x2160", "--size-multipliers=1,4", "--camera-modes=stationary",
        ], repository.Root);

        Assert.AreEqual(0, exit_code);
        Assert.AreEqual("run", runner.Requests.Single().Arguments[0]);
    }

    private static GpuStarfieldBenchmarkCommand.Request Request(int repeats = 1, IReadOnlyList<bool>? camera_modes = null)
    {
        return new GpuStarfieldBenchmarkCommand.Request("editor.exe", "project.uproject", "output", [100], [new GpuStarfieldBenchmarkCommand.Configuration(1280, 720, 1)], camera_modes ?? [false], 1, 5, repeats, 1, 10);
    }

    private static GpuStarfieldBenchmarkCommand.Capture Capture(GpuStarfieldBenchmarkCommand.Configuration configuration, int count, int repeat, bool enabled, bool moving, double value, double? primitive_minimum = null)
    {
        var medians = Metrics(value);
        medians["translucency_draw_calls"] = enabled ? 2 : 0;
        medians["primitives_drawn"] = enabled ? count * 2 + 2 : 0;
        return new GpuStarfieldBenchmarkCommand.Capture(configuration, count, repeat, enabled, moving, 5, medians, new Dictionary<string, double>(medians) { ["primitives_drawn"] = primitive_minimum ?? medians["primitives_drawn"] }, new Dictionary<string, double>(medians));
    }

    private static IReadOnlyList<GpuStarfieldBenchmarkCommand.Delta> Deltas(GpuStarfieldBenchmarkCommand.Request request, double draw_delta, double primitive_delta)
    {
        var result = new List<GpuStarfieldBenchmarkCommand.Delta>();
        foreach (var metric in new[] { "game_thread_ms", "render_thread_ms", "gpu_ms", "translucency_gpu_ms", "starfield_submit_cpu_ms", "rhi_draw_calls", "translucency_draw_calls", "primitives_drawn" })
        {
            var delta = metric switch { "translucency_draw_calls" => draw_delta, "primitives_drawn" => primitive_delta, _ => .5 };
            result.Add(new GpuStarfieldBenchmarkCommand.Delta(request.Configurations.Single(), 100, request.CameraModes.Single(), metric, 1, metric == "starfield_submit_cpu_ms" ? 1 : 1 + delta, delta, delta - .1, delta + .1));
        }
        return result;
    }

    private static Dictionary<string, double> Metrics(double value)
    {
        return new Dictionary<string, double>
        {
            ["game_thread_ms"] = value,
            ["render_thread_ms"] = value,
            ["gpu_ms"] = value,
            ["translucency_gpu_ms"] = value,
            ["starfield_submit_cpu_ms"] = value,
            ["rhi_draw_calls"] = value,
            ["translucency_draw_calls"] = value,
            ["primitives_drawn"] = value,
        };
    }

    private sealed class CaptureFixture : IDisposable
    {
        public CaptureFixture()
        {
            Path = System.IO.Path.Combine(System.IO.Path.GetTempPath(), $"GpuStarfieldCapture{Guid.NewGuid():N}.csv");
        }

        public string Path { get; }
        public void Write(string content) => File.WriteAllText(Path, content);
        public void Dispose() => File.Delete(Path);
    }

    private sealed class TestRepository : IDisposable
    {
        public TestRepository()
        {
            Root = System.IO.Path.Combine(System.IO.Path.GetTempPath(), $"GpuStarfieldRepository{Guid.NewGuid():N}");
            Directory.CreateDirectory(Root);
            Directory.CreateDirectory(System.IO.Path.Combine(Root, ".git"));
            File.WriteAllText(System.IO.Path.Combine(Root, "CMakeLists.txt"), string.Empty);
        }

        public string Root { get; }
        public string CreateFile(string relative_path)
        {
            var path = System.IO.Path.Combine(Root, relative_path);
            Directory.CreateDirectory(System.IO.Path.GetDirectoryName(path)!);
            File.WriteAllText(path, string.Empty);
            return path;
        }
        public void Dispose() => Directory.Delete(Root, recursive: true);
    }

    private sealed class RecordingRunner : IProcessRunner
    {
        public List<ProcessRequest> Requests { get; } = [];
        public Task<ProcessResult> RunAsync(ProcessRequest request, CancellationToken cancellation_token)
        {
            Requests.Add(request);
            return Task.FromResult(new ProcessResult(0));
        }
    }

    private sealed class FakeJobserverLocator : IJobserverLocator { public string Locate() => "jobserver.exe"; }
    private sealed class EmptyEnvironment : IEnvironment { public string? GetEnvironmentVariable(string variable_name) => null; }
}
