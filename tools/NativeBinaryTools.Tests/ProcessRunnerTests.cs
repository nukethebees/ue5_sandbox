using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace NativeBinaryTools.Tests;

[TestClass]
public sealed class ProcessRunnerTests
{
    [TestMethod]
    public async Task RunAsync_reports_a_process_launch_failure()
    {
        var runner = new ProcessRunner();

        await Assert.ThrowsExceptionAsync<ProcessLaunchException>(
            () => runner.RunAsync(
                new ProcessRequest("NativeBinaryTools-does-not-exist.exe", [], Environment.CurrentDirectory, TimeSpan.FromSeconds(1)),
                CancellationToken.None));
    }

    [TestMethod]
    public async Task RunAsync_terminates_a_timed_out_process_tree()
    {
        var runner = new ProcessRunner();

        await Assert.ThrowsExceptionAsync<ProcessTimeoutException>(
            () => runner.RunAsync(
                new ProcessRequest("cmd.exe", ["/c", "ping -n 6 127.0.0.1 > nul"], Environment.CurrentDirectory, TimeSpan.FromMilliseconds(100)),
                CancellationToken.None));
    }
}
