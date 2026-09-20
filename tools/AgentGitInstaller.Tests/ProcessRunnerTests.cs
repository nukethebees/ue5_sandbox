using System.Diagnostics;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace AgentGitInstaller.Tests;

[TestClass]
public sealed class ProcessRunnerTests
{
    [TestMethod]
    public async Task Timeout_terminates_spawned_process()
    {
        using var child = new LongRunningChild();
        var run_task = new ProcessRunner().RunAsync(
            child.CreateRequest(TimeSpan.FromSeconds(2)),
            CancellationToken.None);
        var process_id = await child.WaitForProcessIdAsync(run_task);

        var exception = await Assert.ThrowsExceptionAsync<InstallerException>(async () => await run_task);

        StringAssert.Contains(exception.Message, "timed out after 2 seconds");
        AssertProcessExited(process_id);
    }

    [TestMethod]
    public async Task Caller_cancellation_terminates_spawned_process_and_remains_cancellation()
    {
        using var child = new LongRunningChild();
        using var cancellation = new CancellationTokenSource();
        var run_task = new ProcessRunner().RunAsync(
            child.CreateRequest(TimeSpan.FromSeconds(30)),
            cancellation.Token);
        var process_id = await child.WaitForProcessIdAsync(run_task);

        cancellation.Cancel();
        await Assert.ThrowsExceptionAsync<OperationCanceledException>(async () => await run_task);

        Assert.IsTrue(cancellation.IsCancellationRequested);
        AssertProcessExited(process_id);
    }

    [TestMethod]
    public async Task Normally_completing_process_returns_captured_output()
    {
        var shell = new ExecutableLocator().Find(OperatingSystem.IsWindows() ? "pwsh.exe" : "pwsh");
        var result = await new ProcessRunner().RunAsync(
            new ProcessRequest(
                shell,
                [
                    "-NoProfile",
                    "-NonInteractive",
                    "-Command",
                    "[Console]::Out.Write('complete'); [Console]::Error.Write('diagnostic')",
                ],
                Path.GetTempPath(),
                Timeout: TimeSpan.FromSeconds(10)),
            CancellationToken.None);

        Assert.AreEqual(0, result.ExitCode);
        Assert.AreEqual("complete", result.StandardOutput);
        Assert.AreEqual("diagnostic", result.StandardError);
    }

    private static void AssertProcessExited(int process_id)
    {
        try
        {
            using var process = Process.GetProcessById(process_id);
            Assert.IsTrue(process.HasExited, $"Process {process_id} was still running after cancellation cleanup.");
        }
        catch (ArgumentException)
        {
            // The process no longer exists.
        }
    }

    private sealed class LongRunningChild : IDisposable
    {
        private readonly string root = Directory.CreateTempSubdirectory("AgentGitInstallerRunner-").FullName;
        private int? process_id;

        public ProcessRequest CreateRequest(TimeSpan timeout)
        {
            var shell = new ExecutableLocator().Find(OperatingSystem.IsWindows() ? "pwsh.exe" : "pwsh");
            var escaped_ready_path = ReadyPath.Replace("'", "''", StringComparison.Ordinal);
            return new ProcessRequest(
                shell,
                [
                    "-NoProfile",
                    "-NonInteractive",
                    "-Command",
                    $"[IO.File]::WriteAllText('{escaped_ready_path}', [string]$PID); " +
                    "while ($true) { Start-Sleep -Milliseconds 100 }",
                ],
                root,
                Timeout: timeout);
        }

        public async Task<int> WaitForProcessIdAsync(Task<ProcessResult> run_task)
        {
            using var timeout = new CancellationTokenSource(TimeSpan.FromSeconds(5));
            while (!timeout.IsCancellationRequested)
            {
                if (File.Exists(ReadyPath) &&
                    int.TryParse(await File.ReadAllTextAsync(ReadyPath, timeout.Token), out var parsed_process_id))
                {
                    process_id = parsed_process_id;
                    return parsed_process_id;
                }

                if (run_task.IsCompleted)
                {
                    await run_task;
                    Assert.Fail("The child process exited before publishing its process ID.");
                }

                await Task.Delay(20, timeout.Token);
            }

            throw new AssertFailedException("The child process did not publish its process ID within five seconds.");
        }

        public void Dispose()
        {
            if (process_id.HasValue)
            {
                try
                {
                    using var process = Process.GetProcessById(process_id.Value);
                    if (!process.HasExited)
                    {
                        process.Kill(entireProcessTree: true);
                        process.WaitForExit();
                    }
                }
                catch (ArgumentException)
                {
                }
            }

            if (Directory.Exists(root))
            {
                Directory.Delete(root, recursive: true);
            }
        }

        private string ReadyPath => Path.Combine(root, "ready.txt");
    }
}
