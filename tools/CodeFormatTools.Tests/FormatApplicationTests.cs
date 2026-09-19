using System.Collections.Concurrent;
using CodeFormatTools;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace CodeFormatTools.Tests;

[TestClass]
public sealed class FormatApplicationTests
{
    [TestMethod]
    public async Task RunAsync_restages_only_successfully_formatted_staged_files()
    {
        using var fixture = new TemporaryGitRepository();
        var staged = fixture.WriteFile("Source/staged.cpp", "int old;\n");
        fixture.RunGit("add", "Source/staged.cpp");
        fixture.WriteFile("Source/unrelated.cpp", "int unrelated;\n");
        var formatter = new FakeFormatter();
        formatter.OnFormat = file_path => File.WriteAllText(file_path, "int formatted;\n");
        var output = new StringWriter();
        var error = new StringWriter();
        var application = CreateApplication(formatter, output, error);

        var exit_code = await application.RunAsync(["--staged"], fixture.Root);

        Assert.AreEqual(0, exit_code, error.ToString());
        Assert.AreEqual("int formatted;\n", fixture.RunGit("show", ":Source/staged.cpp"));
        var staged_paths = fixture.RunGit("diff", "--cached", "--name-only");
        Assert.AreEqual("Source/staged.cpp\n", staged_paths);
        var status = fixture.RunGit("status", "--porcelain");
        StringAssert.Contains(status, "?? Source/unrelated.cpp");
    }

    [TestMethod]
    public async Task RunAsync_rejects_staged_files_with_unstaged_edits_before_formatting()
    {
        using var fixture = new TemporaryGitRepository();
        fixture.WriteFile("Source/mixed.cpp");
        fixture.CommitAll();
        fixture.WriteFile("Source/mixed.cpp", "int staged;\n");
        fixture.RunGit("add", "Source/mixed.cpp");
        fixture.WriteFile("Source/mixed.cpp", "int unstaged;\n");
        var formatter = new FakeFormatter();
        var error = new StringWriter();
        var application = CreateApplication(formatter, new StringWriter(), error);

        var exit_code = await application.RunAsync(["--staged"], fixture.Root);

        Assert.AreEqual(1, exit_code);
        Assert.AreEqual(0, formatter.Attempts.Count);
        StringAssert.Contains(error.ToString(), Path.Combine("Source", "mixed.cpp"));
        StringAssert.Contains(error.ToString(), "Stage or stash");
    }

    [TestMethod]
    public async Task RunAsync_continues_after_formatter_failure_and_returns_failure()
    {
        using var fixture = new TemporaryGitRepository();
        var failed = fixture.WriteFile("Source/failed.cpp");
        var succeeded = fixture.WriteFile("Source/succeeded.cpp");
        var formatter = new FakeFormatter();
        formatter.Failures[failed] = "formatter diagnostic";
        var error = new StringWriter();
        var application = CreateApplication(formatter, new StringWriter(), error);

        var exit_code = await application.RunAsync(["--all"], fixture.Root);

        Assert.AreEqual(1, exit_code);
        CollectionAssert.AreEquivalent(new[] { failed, succeeded }, formatter.Attempts.ToArray());
        StringAssert.Contains(error.ToString(), "formatter diagnostic");
        StringAssert.Contains(error.ToString(), "Errors encountered");
    }

    [TestMethod]
    public async Task RunAsync_reports_git_selection_failures_without_starting_formatter()
    {
        var formatter = new FakeFormatter();
        var output = new StringWriter();
        var error = new StringWriter();
        var runner = new DelegatingProcessRunner(_ => new ProcessResult(7, [], "Git unavailable"));
        var application = new FormatApplication(
            new FormatFileSelector(new GitFileSelector(runner), ["Source"]),
            formatter,
            output,
            error);

        var exit_code = await application.RunAsync(["--changed"], Environment.CurrentDirectory);

        Assert.AreEqual(1, exit_code);
        Assert.IsFalse(formatter.AvailabilityChecked);
        StringAssert.Contains(error.ToString(), "Git unavailable");
    }

    [TestMethod]
    public async Task RunAsync_returns_usage_error_for_invalid_arguments()
    {
        using var fixture = new TemporaryGitRepository();
        var error = new StringWriter();
        var application = CreateApplication(new FakeFormatter(), new StringWriter(), error);

        var exit_code = await application.RunAsync(["--all", "--changed"], fixture.Root);

        Assert.AreEqual(2, exit_code);
        StringAssert.Contains(error.ToString(), "Usage:");
    }

    private static FormatApplication CreateApplication(IFileFormatter formatter, TextWriter output, TextWriter error)
    {
        return new FormatApplication(
            new FormatFileSelector(new GitFileSelector(new ProcessRunner()), ["Source"]),
            formatter,
            output,
            error);
    }

    private sealed class FakeFormatter : IFileFormatter
    {
        private int availability_checked;

        public ConcurrentQueue<string> Attempts { get; } = [];

        public ConcurrentDictionary<string, string> Failures { get; } = new(StringComparer.OrdinalIgnoreCase);

        public Action<string>? OnFormat { get; set; }

        public bool AvailabilityChecked => Volatile.Read(ref availability_checked) != 0;

        public Task EnsureAvailableAsync(CancellationToken cancellation_token)
        {
            Interlocked.Exchange(ref availability_checked, 1);
            return Task.CompletedTask;
        }

        public Task<FormatFileResult> FormatAsync(string file_path, CancellationToken cancellation_token)
        {
            Attempts.Enqueue(file_path);
            if (Failures.TryGetValue(file_path, out var error))
            {
                return Task.FromResult(new FormatFileResult(false, error));
            }

            OnFormat?.Invoke(file_path);
            return Task.FromResult(new FormatFileResult(true, null));
        }
    }

    private sealed class DelegatingProcessRunner(Func<ProcessRequest, ProcessResult> run) : IProcessRunner
    {
        public Task<ProcessResult> RunAsync(ProcessRequest request, CancellationToken cancellation_token)
        {
            return Task.FromResult(run(request));
        }
    }
}
