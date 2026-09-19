using CodeFormatTools;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace CodeFormatTools.Tests;

[TestClass]
public sealed class ClangFormatterTests
{
    [TestMethod]
    public async Task FormatAsync_invokes_clang_format_with_argument_boundaries_and_normalizes_newlines()
    {
        using var fixture = new TemporaryGitRepository();
        var file = fixture.WriteFile("Source/file with spaces.cpp", "int main() {\r\n}\r");
        var runner = new RecordingProcessRunner();
        var formatter = new ClangFormatter(runner);

        await formatter.EnsureAvailableAsync(CancellationToken.None);
        var result = await formatter.FormatAsync(file, CancellationToken.None);

        Assert.IsTrue(result.Success, result.Error);
        CollectionAssert.AreEqual(new[] { "--version" }, runner.Requests[0].Arguments.ToArray());
        CollectionAssert.AreEqual(new[] { "-i", file }, runner.Requests[1].Arguments.ToArray());
        CollectionAssert.AreEqual("int main() {\n}\n"u8.ToArray(), File.ReadAllBytes(file));
    }

    [TestMethod]
    public async Task FormatAsync_returns_formatter_stderr_as_a_file_failure()
    {
        using var fixture = new TemporaryGitRepository();
        var file = fixture.WriteFile("Source/fail.cpp");
        var runner = new RecordingProcessRunner
        {
            Response = request => request.Arguments.Contains("-i")
                ? new ProcessResult(1, [], "bad formatting")
                : new ProcessResult(0, [], string.Empty),
        };
        var formatter = new ClangFormatter(runner);

        var result = await formatter.FormatAsync(file, CancellationToken.None);

        Assert.IsFalse(result.Success);
        Assert.AreEqual("bad formatting", result.Error);
    }

    [TestMethod]
    public async Task EnsureAvailableAsync_reports_a_missing_clang_format()
    {
        var formatter = new ClangFormatter(new ThrowingProcessRunner());

        var exception = await Assert.ThrowsExceptionAsync<FormatToolException>(
            () => formatter.EnsureAvailableAsync(CancellationToken.None));

        StringAssert.Contains(exception.Message, "available on PATH");
    }

    private sealed class RecordingProcessRunner : IProcessRunner
    {
        public List<ProcessRequest> Requests { get; } = [];

        public Func<ProcessRequest, ProcessResult> Response { get; set; } = _ => new ProcessResult(0, [], string.Empty);

        public Task<ProcessResult> RunAsync(ProcessRequest request, CancellationToken cancellation_token)
        {
            Requests.Add(request);
            return Task.FromResult(Response(request));
        }
    }

    private sealed class ThrowingProcessRunner : IProcessRunner
    {
        public Task<ProcessResult> RunAsync(ProcessRequest request, CancellationToken cancellation_token)
        {
            throw new ProcessLaunchException("Unable to start 'clang-format'.", new InvalidOperationException());
        }
    }
}
