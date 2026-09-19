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
            var git_path = Path.Combine(root, "git", "trusted git.exe");
            var lfs_path = Path.Combine(root, "lfs dir's", "git-lfs.exe");
            Directory.CreateDirectory(Path.GetDirectoryName(git_path)!);
            Directory.CreateDirectory(Path.GetDirectoryName(lfs_path)!);
            File.WriteAllText(git_path, string.Empty);
            File.WriteAllText(lfs_path, string.Empty);
            var trust = new TrustContext(root, git_path, lfs_path, "Test User", "test@example.com", config, hooks, []);
            var client = new GitClient(trust, runner);

            await client.RunAsync(root, ["commit", "-m", "a & b"]);

            Assert.AreEqual(git_path, runner.Request!.FileName);
            Assert.IsTrue(runner.Request.Arguments.Contains("a & b"));
            Assert.AreEqual("0", runner.Request.Environment["GIT_TERMINAL_PROMPT"]);
            Assert.AreEqual("1", runner.Request.Environment["GIT_LFS_SKIP_SMUDGE"]);
            Assert.AreEqual(config, runner.Request.Environment["GIT_CONFIG_GLOBAL"]);
            Assert.AreEqual(
                Path.GetDirectoryName(lfs_path),
                runner.Request.Environment["PATH"].Split(Path.PathSeparator)[0]);
            var normalized_lfs_path = lfs_path.Replace('\\', '/').Replace("'", "'\\''", StringComparison.Ordinal);
            Assert.IsTrue(runner.Request.Arguments.Contains(
                $"filter.lfs.process='{normalized_lfs_path}' filter-process"));
            Assert.IsTrue(runner.Request.Arguments.Contains("filter.lfs.clean="));
            Assert.IsTrue(runner.Request.Arguments.Contains("filter.lfs.smudge="));
            foreach (var unsafe_variable in new[]
                     {
                         "GIT_DIR",
                         "GIT_WORK_TREE",
                         "GIT_INDEX_FILE",
                         "GIT_OBJECT_DIRECTORY",
                         "GIT_CONFIG_COUNT",
                         "GIT_EXEC_PATH",
                     })
            {
                Assert.IsFalse(runner.Request.Environment.Keys.Any(
                    key => key.Equals(unsafe_variable, StringComparison.OrdinalIgnoreCase)));
            }
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

    [TestMethod]
    public async Task ProcessRunner_bounds_captured_diagnostic_output()
    {
        using var fixture = new TemporaryAgentGitRepository();
        var environment = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase)
        {
            ["SystemRoot"] = Environment.GetFolderPath(Environment.SpecialFolder.Windows),
            ["PATH"] = Path.GetDirectoryName(fixture.GitExecutable)!,
        };
        var request = new ProcessRequest(
            fixture.GitExecutable,
            ["--version"],
            fixture.RepositoryRoot,
            environment,
            TimeSpan.FromSeconds(10),
            MaximumCapturedStreamBytes: 1);

        var exception = await Assert.ThrowsExceptionAsync<RepositoryStateException>(
            async () => await new ProcessRunner().RunAsync(request, CancellationToken.None));

        StringAssert.Contains(exception.Message, "1 byte diagnostic output limit");
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
