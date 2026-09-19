using System.Collections.Concurrent;
using CodeFormatTools;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace CodeFormatTools.Tests;

[TestClass]
public sealed class ParallelFormattingTests
{
    [TestMethod]
    public async Task RunAsync_formats_files_in_parallel_with_bounded_concurrency_and_one_attempt_per_file()
    {
        using var fixture = new TemporaryGitRepository();
        var files = CreateFiles(fixture, 6);
        var formatter = new ControlledFormatter(expected_starts: 3);
        var application = CreateApplication(formatter, new StringWriter(), new StringWriter());

        var run_task = application.RunAsync(["--all", "--jobs", "3"], fixture.Root);
        await formatter.WaitForStartsAsync(3);

        Assert.AreEqual(3, formatter.MaximumActive);
        formatter.CompleteAll(files, new FormatFileResult(true, null));

        var exit_code = await run_task;

        Assert.AreEqual(0, exit_code);
        Assert.IsTrue(formatter.MaximumActive <= 3);
        CollectionAssert.AreEquivalent(files.ToArray(), formatter.Attempts.ToArray());
    }

    [TestMethod]
    public async Task RunAsync_with_one_job_never_formats_more_than_one_file_at_a_time()
    {
        using var fixture = new TemporaryGitRepository();
        var files = CreateFiles(fixture, 4);
        var formatter = new ControlledFormatter(expected_starts: 1);
        var application = CreateApplication(formatter, new StringWriter(), new StringWriter());

        var run_task = application.RunAsync(["--all", "-j", "1"], fixture.Root);
        for (var index = 0; index < files.Count; index++)
        {
            await formatter.WaitForStartsAsync(index + 1);
            Assert.IsTrue(formatter.MaximumActive <= 1);
            formatter.Complete(formatter.Attempts.ToArray()[index], new FormatFileResult(true, null));
        }

        var exit_code = await run_task;

        Assert.AreEqual(0, exit_code);
        Assert.AreEqual(1, formatter.MaximumActive);
    }

    [TestMethod]
    public async Task RunAsync_reports_results_in_selection_order_when_formatting_completes_out_of_order()
    {
        using var fixture = new TemporaryGitRepository();
        var files = CreateFiles(fixture, 3);
        var output = new StringWriter();
        var error = new StringWriter();
        var formatter = new ControlledFormatter(expected_starts: 3);
        var application = CreateApplication(formatter, output, error);

        var run_task = application.RunAsync(["--all", "--jobs", "3", "--verbose"], fixture.Root);
        await formatter.WaitForStartsAsync(3);

        formatter.Complete(files[2], new FormatFileResult(false, "third failure"));
        formatter.Complete(files[1], new FormatFileResult(false, "second failure"));
        formatter.Complete(files[0], new FormatFileResult(true, null));

        var exit_code = await run_task;

        Assert.AreEqual(1, exit_code);
        AssertInOrder(
            output.ToString(),
            $"Formatting: {RelativePath(fixture, files[0])}",
            $"Formatting: {RelativePath(fixture, files[1])}",
            $"Formatting: {RelativePath(fixture, files[2])}");
        AssertInOrder(
            error.ToString(),
            $"ERROR formatting {RelativePath(fixture, files[1])}: second failure",
            $"ERROR formatting {RelativePath(fixture, files[2])}: third failure",
            $"  {RelativePath(fixture, files[1])}: second failure",
            $"  {RelativePath(fixture, files[2])}: third failure");
        CollectionAssert.AreEquivalent(files.ToArray(), formatter.Attempts.ToArray());
    }

    [TestMethod]
    public async Task RunAsync_stages_only_after_every_parallel_format_succeeds()
    {
        using var fixture = new TemporaryGitRepository();
        var files = CreateFiles(fixture, 2);
        fixture.RunGit("add", "Source/file-0.cpp", "Source/file-1.cpp");
        var formatter = new ControlledFormatter(
            expected_starts: 2,
            on_success: file_path => File.WriteAllText(file_path, $"formatted {Path.GetFileName(file_path)}\n"));
        var application = CreateApplication(formatter, new StringWriter(), new StringWriter());

        var run_task = application.RunAsync(["--staged", "--jobs", "2"], fixture.Root);
        await formatter.WaitForStartsAsync(2);

        formatter.Complete(files[0], new FormatFileResult(true, null));
        await formatter.WaitForReturnAsync(files[0]);
        Assert.AreEqual("int file_0;\n", fixture.RunGit("show", ":Source/file-0.cpp"));

        formatter.Complete(files[1], new FormatFileResult(true, null));
        var exit_code = await run_task;

        Assert.AreEqual(0, exit_code);
        Assert.AreEqual("formatted file-0.cpp\n", fixture.RunGit("show", ":Source/file-0.cpp"));
        Assert.AreEqual("formatted file-1.cpp\n", fixture.RunGit("show", ":Source/file-1.cpp"));
    }

    [TestMethod]
    public async Task RunAsync_does_not_stage_successful_files_when_another_staged_file_fails()
    {
        using var fixture = new TemporaryGitRepository();
        var files = CreateFiles(fixture, 2);
        fixture.RunGit("add", "Source/file-0.cpp", "Source/file-1.cpp");
        var formatter = new ControlledFormatter(
            expected_starts: 2,
            on_success: file_path => File.WriteAllText(file_path, $"formatted {Path.GetFileName(file_path)}\n"));
        var application = CreateApplication(formatter, new StringWriter(), new StringWriter());

        var run_task = application.RunAsync(["--staged", "--jobs", "2"], fixture.Root);
        await formatter.WaitForStartsAsync(2);

        formatter.Complete(files[1], new FormatFileResult(true, null));
        formatter.Complete(files[0], new FormatFileResult(false, "formatter diagnostic"));

        var exit_code = await run_task;

        Assert.AreEqual(1, exit_code);
        Assert.AreEqual("int file_1;\n", fixture.RunGit("show", ":Source/file-1.cpp"));
        CollectionAssert.AreEquivalent(files.ToArray(), formatter.Attempts.ToArray());
    }

    [TestMethod]
    public async Task RunAsync_propagates_cancellation_without_staging_files()
    {
        using var fixture = new TemporaryGitRepository();
        var files = CreateFiles(fixture, 2);
        fixture.RunGit("add", "Source/file-0.cpp", "Source/file-1.cpp");
        var formatter = new ControlledFormatter(expected_starts: 2);
        var application = CreateApplication(formatter, new StringWriter(), new StringWriter());
        using var cancellation = new CancellationTokenSource();

        var run_task = application.RunAsync(["--staged", "--jobs", "2"], fixture.Root, cancellation.Token);
        await formatter.WaitForStartsAsync(2);
        cancellation.Cancel();

        try
        {
            await run_task;
            Assert.Fail("Cancellation should propagate from the formatter.");
        }
        catch (OperationCanceledException)
        {
        }

        Assert.AreEqual(2, formatter.CancellationCount);
        Assert.AreEqual("int file_0;\n", fixture.RunGit("show", ":Source/file-0.cpp"));
        Assert.AreEqual("int file_1;\n", fixture.RunGit("show", ":Source/file-1.cpp"));
    }

    private static IReadOnlyList<string> CreateFiles(TemporaryGitRepository fixture, int count)
    {
        return Enumerable.Range(0, count)
            .Select(index => fixture.WriteFile($"Source/file-{index}.cpp", $"int file_{index};\n"))
            .ToArray();
    }

    private static FormatApplication CreateApplication(IFileFormatter formatter, TextWriter output, TextWriter error)
    {
        return new FormatApplication(
            new FormatFileSelector(new GitFileSelector(new ProcessRunner()), ["Source"]),
            formatter,
            output,
            error);
    }

    private static string RelativePath(TemporaryGitRepository fixture, string file_path)
    {
        return Path.GetRelativePath(fixture.Root, file_path);
    }

    private static void AssertInOrder(string text, params string[] expected_fragments)
    {
        var start = 0;
        foreach (var expected in expected_fragments)
        {
            var position = text.IndexOf(expected, start, StringComparison.Ordinal);
            Assert.IsTrue(position >= 0, $"Expected '{expected}' after position {start} in:{Environment.NewLine}{text}");
            start = position + expected.Length;
        }
    }

    private sealed class ControlledFormatter(int expected_starts, Action<string>? on_success = null) : IFileFormatter
    {
        private readonly ConcurrentDictionary<string, TaskCompletionSource<FormatFileResult>> completions = new(StringComparer.OrdinalIgnoreCase);
        private readonly ConcurrentDictionary<string, FormatFileResult> completed_results = new(StringComparer.OrdinalIgnoreCase);
        private readonly ConcurrentDictionary<string, TaskCompletionSource> returns = new(StringComparer.OrdinalIgnoreCase);
        private readonly ConcurrentDictionary<int, TaskCompletionSource> start_notifications = [];
        private int active;
        private int cancellation_count;
        private int maximum_active;
        private int started;

        public ConcurrentQueue<string> Attempts { get; } = [];

        public int CancellationCount => Volatile.Read(ref cancellation_count);

        public int MaximumActive => Volatile.Read(ref maximum_active);

        public Task EnsureAvailableAsync(CancellationToken cancellation_token)
        {
            return Task.CompletedTask;
        }

        public async Task<FormatFileResult> FormatAsync(string file_path, CancellationToken cancellation_token)
        {
            var completion = completions.GetOrAdd(
                file_path,
                _ => new TaskCompletionSource<FormatFileResult>(TaskCreationOptions.RunContinuationsAsynchronously));
            var returned = returns.GetOrAdd(
                file_path,
                _ => new TaskCompletionSource(TaskCreationOptions.RunContinuationsAsynchronously));
            if (completed_results.TryGetValue(file_path, out var completed_result))
            {
                completion.TrySetResult(completed_result);
            }

            Attempts.Enqueue(file_path);
            RecordActive(Interlocked.Increment(ref active));
            NotifyStarted(Interlocked.Increment(ref started));

            try
            {
                var result = await completion.Task.WaitAsync(cancellation_token);
                if (result.Success)
                {
                    on_success?.Invoke(file_path);
                }

                return result;
            }
            catch (OperationCanceledException) when (cancellation_token.IsCancellationRequested)
            {
                Interlocked.Increment(ref cancellation_count);
                throw;
            }
            finally
            {
                Interlocked.Decrement(ref active);
                returned.TrySetResult();
            }
        }

        public void Complete(string file_path, FormatFileResult result)
        {
            completed_results[file_path] = result;
            if (completions.TryGetValue(file_path, out var completion))
            {
                completion.TrySetResult(result);
            }
        }

        public void CompleteAll(IEnumerable<string> file_paths, FormatFileResult result)
        {
            foreach (var file_path in file_paths)
            {
                Complete(file_path, result);
            }
        }

        public async Task WaitForReturnAsync(string file_path)
        {
            await returns[file_path].Task.WaitAsync(TimeSpan.FromSeconds(10));
        }

        public async Task WaitForStartsAsync(int count)
        {
            var notification = start_notifications.GetOrAdd(
                count,
                _ => new TaskCompletionSource(TaskCreationOptions.RunContinuationsAsynchronously));
            if (Volatile.Read(ref started) >= count)
            {
                notification.TrySetResult();
            }

            await notification.Task.WaitAsync(TimeSpan.FromSeconds(10));
        }

        private void NotifyStarted(int current_started)
        {
            foreach (var notification in start_notifications)
            {
                if (notification.Key <= current_started)
                {
                    notification.Value.TrySetResult();
                }
            }

            if (current_started >= expected_starts)
            {
                start_notifications.GetOrAdd(
                    expected_starts,
                    _ => new TaskCompletionSource(TaskCreationOptions.RunContinuationsAsynchronously)).TrySetResult();
            }
        }

        private void RecordActive(int current_active)
        {
            while (true)
            {
                var observed_maximum = Volatile.Read(ref maximum_active);
                if (current_active <= observed_maximum ||
                    Interlocked.CompareExchange(ref maximum_active, current_active, observed_maximum) == observed_maximum)
                {
                    return;
                }
            }
        }
    }
}
