using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace BenchmarkTools.Tests;

[TestClass]
public sealed class ProcessRunnerTests
{
    [TestMethod]
    public async Task Process_log_preserves_stdout_and_stderr_on_success()
    {
        using var directory = new BenchmarkTestDirectory();
        var request = Request(directory, false);
        var result = await new ProcessRunner().RunAsync(request, default);
        Assert.AreEqual(0, result.ExitCode);
        StringAssert.Contains(result.StandardOutput, "stdout-marker");
        StringAssert.Contains(result.StandardError, "stderr-marker");
        AssertLog(request);
    }

    [TestMethod]
    public async Task Timeout_retains_process_output()
    {
        using var directory = new BenchmarkTestDirectory();
        var request = Request(directory, true) with { Timeout = TimeSpan.FromSeconds(2) };
        await Assert.ThrowsExceptionAsync<ProcessTimeoutException>(() => new ProcessRunner().RunAsync(request, default));
        AssertLog(request);
    }

    [TestMethod]
    public async Task Output_is_published_before_exit_and_survives_cancellation()
    {
        using var directory = new BenchmarkTestDirectory();
        var request = Request(directory, true);
        using var cancellation = new CancellationTokenSource(TimeSpan.FromSeconds(10));
        var running = new ProcessRunner().RunAsync(request, cancellation.Token);
        try
        {
            while (!File.ReadAllText(request.OutputLogPath!).Contains("stderr-marker", StringComparison.Ordinal))
                await Task.Delay(20, cancellation.Token);
            Assert.IsFalse(running.IsCompleted);
        }
        finally
        {
            cancellation.Cancel();
            try
            {
                await running;
                Assert.Fail("The process should have been cancelled.");
            }
            catch (OperationCanceledException) { }
        }
        AssertLog(request);
    }

    private static ProcessRequest Request(BenchmarkTestDirectory directory, bool wait)
    {
        var script = Path.Combine(directory.Root, "output.cmd");
        File.WriteAllText(script, "@echo off\r\necho stdout-marker\r\necho stderr-marker 1>&2\r\n" +
            (wait ? "ping -n 30 127.0.0.1 >nul\r\n" : ""));
        return new ProcessRequest(Environment.GetEnvironmentVariable("ComSpec") ?? "cmd.exe", ["/d", "/c", script],
            directory.Root, OutputLogPath: Path.Combine(directory.Root, "process.log"));
    }

    private static void AssertLog(ProcessRequest request)
    {
        var log = File.ReadAllText(request.OutputLogPath!);
        StringAssert.Contains(log, "stdout-marker");
        StringAssert.Contains(log, "stderr-marker");
    }
}
