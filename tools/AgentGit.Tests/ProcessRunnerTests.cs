using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace AgentGit.Tests;

[TestClass]
public sealed class ProcessRunnerTests
{
    [TestMethod]
    public async Task GitClient_uses_pinned_executable_structured_arguments_and_sanitized_environment()
    {
        var runner = new RecordingRunner();
        var root = Directory.CreateTempSubdirectory("AgentGitProcess-").FullName;
        try
        {
            var config = Path.Combine(root, "empty.gitconfig");
            var hooks = Directory.CreateDirectory(Path.Combine(root, "hooks")).FullName;
            File.WriteAllText(config, string.Empty);
            var git_path = Path.Combine(root, "trusted git.exe");
            File.WriteAllText(git_path, string.Empty);
            var trust = new TrustContext(root, git_path, null, "Test User", "test@example.com", config, hooks, []);
            var client = new GitClient(trust, runner);

            await client.RunAsync(root, ["commit", "-m", "a & b"]);

            Assert.AreEqual(git_path, runner.Request!.FileName);
            Assert.IsTrue(runner.Request.Arguments.Contains("a & b"));
            Assert.AreEqual("0", runner.Request.Environment["GIT_TERMINAL_PROMPT"]);
            Assert.AreEqual("1", runner.Request.Environment["GIT_LFS_SKIP_SMUDGE"]);
            Assert.AreEqual(config, runner.Request.Environment["GIT_CONFIG_GLOBAL"]);
            Assert.IsFalse(runner.Request.Environment.Keys.Any(key => key.Equals("GIT_DIR", StringComparison.OrdinalIgnoreCase)));
        }
        finally
        {
            Directory.Delete(root, recursive: true);
        }
    }

    [TestMethod]
    public void CreateStartInfo_never_uses_a_shell_and_preserves_argument_boundaries()
    {
        var request = new ProcessRequest(
            "C:/trusted/git.exe",
            ["commit", "-m", "value & whoami"],
            Environment.CurrentDirectory,
            new Dictionary<string, string> { ["SAFE"] = "1" },
            TimeSpan.FromSeconds(1));

        var start_info = ProcessRunner.CreateStartInfo(request);

        Assert.IsFalse(start_info.UseShellExecute);
        Assert.IsTrue(start_info.RedirectStandardOutput);
        Assert.IsTrue(start_info.RedirectStandardError);
        CollectionAssert.AreEqual(request.Arguments.ToArray(), start_info.ArgumentList.ToArray());
        Assert.AreEqual("1", start_info.Environment["SAFE"]);
        Assert.AreEqual(1, start_info.Environment.Count);
    }

    private sealed class RecordingRunner : IProcessRunner
    {
        public ProcessRequest? Request { get; private set; }

        public Task<ProcessResult> RunAsync(ProcessRequest request, CancellationToken cancellation_token)
        {
            Request = request;
            return Task.FromResult(new ProcessResult(0, [], string.Empty));
        }
    }
}
