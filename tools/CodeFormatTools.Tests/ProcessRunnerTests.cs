using CodeFormatTools;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace CodeFormatTools.Tests;

[TestClass]
public sealed class ProcessRunnerTests
{
    [TestMethod]
    public async Task RunAsync_terminates_a_running_process_when_cancelled()
    {
        var runner = new ProcessRunner();
        using var cancellation = new CancellationTokenSource();
        var run_task = runner.RunAsync(
            new ProcessRequest(
                "cmd.exe",
                ["/c", "ping -n 30 127.0.0.1 > nul"],
                Environment.CurrentDirectory,
                TimeSpan.FromMinutes(1)),
            cancellation.Token);

        await Task.Delay(TimeSpan.FromMilliseconds(100));
        cancellation.Cancel();

        var completed_task = await Task.WhenAny(run_task, Task.Delay(TimeSpan.FromSeconds(5)));
        Assert.AreSame(run_task, completed_task, "Cancelled process did not terminate promptly.");
        try
        {
            await run_task;
            Assert.Fail("Cancellation should propagate from ProcessRunner.");
        }
        catch (OperationCanceledException)
        {
        }
    }
}
