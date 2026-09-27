using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace BenchmarkTools.Tests;

[TestClass]
public sealed class BenchmarkSubmoduleTests
{
    [DataTestMethod]
    [DataRow(false)]
    [DataRow(true)]
    public async Task Seeds_exact_commit_and_lfs_without_copying_candidate_edits_and_fetches_only_missing_data(bool missing_commit)
    {
        using var directory = new BenchmarkTestDirectory();
        var runner = new RecordingGitRunner();
        var app = new BenchmarkToolsApplication(runner, new TestJobserver(), new TestEnvironment(), TextWriter.Null, TextWriter.Null, "unused");
        var candidate = Path.Combine(directory.Root, "candidate");
        var remote = Path.Combine(directory.Root, "origin");
        var local = Path.Combine(candidate, "deps", "library with spaces");
        await Init(remote);
        File.WriteAllText(Path.Combine(remote, ".gitattributes"), "*.bin filter=lfs diff=lfs merge=lfs -text\n");
        await Commit(remote, "baseline");
        var old_commit = await Git(remote, "rev-parse", "HEAD");
        Directory.CreateDirectory(candidate);
        await Git(candidate, "clone", "--no-hardlinks", "--", remote, local);
        await Commit(remote, "remote-only");
        var required = missing_commit ? await Git(remote, "rev-parse", "HEAD") : old_commit;
        await ConfigureAuthor(local);
        await Commit(local, "candidate");
        File.WriteAllText(Path.Combine(local, "value.txt"), "uncommitted candidate edit");
        File.WriteAllText(Path.Combine(local, "asset.bin"), "uncommitted asset edit");
        File.WriteAllText(Path.Combine(local, "untracked.txt"), "do not copy");
        var before = await Git(local, "status", "--porcelain");
        var local_head = await Git(local, "rev-parse", "HEAD");

        await Init(candidate);
        File.WriteAllText(Path.Combine(candidate, ".gitignore"), ".local/\n");
        var url = missing_commit ? remote.Replace('\\', '/') : "https://unavailable.invalid/library.git";
        File.WriteAllText(Path.Combine(candidate, ".gitmodules"), $"[submodule \"library\"]\npath = deps/library with spaces\nurl = {url}\n");
        await Git(candidate, "add", ".gitignore", ".gitmodules");
        await Git(candidate, "update-index", "--add", "--cacheinfo", $"160000,{required},deps/library with spaces");
        await Git(candidate, "commit", "-m", "pinned submodule");
        string baseline;
        await using (var session = await RevisionComparisonSession.CreateAsync(app, new RepositoryPaths(candidate), "HEAD", null, false, default))
        {
            baseline = session.BaselineRoot;
            runner.Requests.Clear();
            await session.InitializeSubmodulesAsync(default);
            var target = Path.Combine(baseline, "deps", "library with spaces");
            Assert.AreEqual(required, await Git(target, "rev-parse", "HEAD"));
            Assert.AreEqual(url, await Git(target, "remote", "get-url", "origin"));
            Assert.AreEqual(missing_commit ? "remote-only" : "baseline", File.ReadAllText(Path.Combine(target, "value.txt")));
            Assert.AreEqual(missing_commit ? "remote-only" : "baseline", File.ReadAllText(Path.Combine(target, "asset.bin")));
            Assert.IsFalse(File.Exists(Path.Combine(target, "untracked.txt")));
            Assert.AreEqual(string.Empty, await Git(target, "status", "--porcelain"));
            Assert.AreEqual(missing_commit ? 1 : 0, runner.Requests.Count(request => request.Arguments.Contains("fetch")));
            Assert.IsFalse(File.Exists(Path.Combine(target, ".git", "objects", "info", "alternates")));
            var detached = await runner.RunAsync(BenchmarkGit.Request(target, ["symbolic-ref", "-q", "HEAD"]), default);
            Assert.AreNotEqual(0, detached.ExitCode);
        }
        Assert.IsFalse(Directory.Exists(baseline));
        Assert.AreEqual(before, await Git(local, "status", "--porcelain"));
        Assert.AreEqual(local_head, await Git(local, "rev-parse", "HEAD"));
        Assert.AreEqual("uncommitted asset edit", File.ReadAllText(Path.Combine(local, "asset.bin")));

        async Task<string> Git(string root, params string[] arguments) => await BenchmarkGit.TextAsync(app, root, arguments, default);
        async Task ConfigureAuthor(string root)
        {
            await Git(root, "config", "user.name", "Benchmark test");
            await Git(root, "config", "user.email", "benchmark@example.invalid");
        }
        async Task Init(string root)
        {
            Directory.CreateDirectory(root);
            await Git(root, "init", "--initial-branch=fixture");
            await ConfigureAuthor(root);
        }
        async Task Commit(string root, string content)
        {
            File.WriteAllText(Path.Combine(root, "value.txt"), content);
            File.WriteAllText(Path.Combine(root, "asset.bin"), content);
            await Git(root, "add", ".");
            await Git(root, "commit", "-m", content);
        }
    }

    private sealed class RecordingGitRunner : IProcessRunner
    {
        private readonly ProcessRunner runner_ = new();
        public List<ProcessRequest> Requests { get; } = [];
        public Task<ProcessResult> RunAsync(ProcessRequest request, CancellationToken token)
        {
            Requests.Add(request);
            return runner_.RunAsync(request, token);
        }
    }
}
