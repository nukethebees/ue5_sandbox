using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace GamePackageTools.Tests;

[TestClass]
public sealed class UnrealPakRunnerTests
{
    [TestMethod]
    public async Task ListPakAsync_preserves_argument_boundaries_for_paths_with_spaces()
    {
        var runner = new RecordingProcessRunner();
        var unrealPak = new UnrealPakRunner(runner, "C:/Tools with spaces/UnrealPak.exe");

        await unrealPak.ListPakAsync("C:/Package with spaces/Sandbox.pak", CancellationToken.None);

        Assert.AreEqual("C:/Tools with spaces/UnrealPak.exe", runner.Requests[0].FileName);
        CollectionAssert.AreEqual(new[] { "C:/Package with spaces/Sandbox.pak", "-List" }, runner.Requests[0].Arguments.ToArray());
    }

    [TestMethod]
    public async Task ListPakAsync_fails_clearly_when_unreal_pak_returns_nonzero()
    {
        var runner = new RecordingProcessRunner { Result = new ProcessResult(9, string.Empty, "bad pak") };
        var unrealPak = new UnrealPakRunner(runner, "UnrealPak.exe");

        var exception = await Assert.ThrowsExceptionAsync<PackageVerificationException>(
            () => unrealPak.ListPakAsync("package.pak", CancellationToken.None));

        StringAssert.Contains(exception.Message, "code 9");
        StringAssert.Contains(exception.Message, "bad pak");
    }

    private sealed class RecordingProcessRunner : IProcessRunner
    {
        public List<ProcessRequest> Requests { get; } = [];

        public ProcessResult Result { get; set; } = new(0, "inventory", string.Empty);

        public Task<ProcessResult> RunAsync(ProcessRequest request, CancellationToken cancellationToken)
        {
            Requests.Add(request);
            return Task.FromResult(Result);
        }
    }
}
