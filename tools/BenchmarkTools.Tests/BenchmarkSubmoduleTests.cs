using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace BenchmarkTools.Tests;

[TestClass]
public sealed class BenchmarkSubmoduleTests
{
    [DataTestMethod]
    [DataRow(false, true)]
    [DataRow(true, true)]
    [DataRow(false, false)]
    [DataRow(true, false)]
    public async Task Seeds_exact_commit_and_lfs_without_copying_candidate_edits_and_fetches_only_missing_data(bool missing_commit, bool lfs)
    {
        using var directory = new BenchmarkTestDirectory();
        var runner = new RecordingGitRunner();
        var app = new BenchmarkToolsApplication(runner, TextWriter.Null, TextWriter.Null, "unused");
        var candidate = Path.Combine(directory.Root, "candidate");
        var remote = Path.Combine(directory.Root, "origin");
        var local = Path.Combine(candidate, "deps", "library with spaces");
        await Init(remote);
        if (lfs) File.WriteAllText(Path.Combine(remote, ".gitattributes"), "*.bin filter=lfs diff=lfs merge=lfs -text\n");
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
        var baseline = Path.Combine(directory.Root, "baseline");
        await Git(directory.Root, "clone", "--no-hardlinks", "--", candidate, baseline);
        {
            runner.Requests.Clear();
            await BenchmarkSubmodules.InitializeAsync(app, baseline, candidate, null, default);
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

    [TestMethod]
    [DataRow("tracked")]
    [DataRow("staged")]
    [DataRow("untracked")]
    public async Task Source_capture_and_verification_reject_dirty_submodules_despite_ignore_settings(string change)
    {
        using var directory = new BenchmarkTestDirectory();
        var app = new BenchmarkToolsApplication(new ProcessRunner(), TextWriter.Null, TextWriter.Null, "unused");
        var module = Path.Combine(directory.Root, "dependency with spaces");
        Directory.CreateDirectory(module);
        await Init(module);
        var tracked = Path.Combine(module, "tracked.txt");
        File.WriteAllText(tracked, "original\n");
        await Git(module, "add", ".");
        await Git(module, "commit", "-m", "dependency");
        var commit = await Git(module, "rev-parse", "HEAD");
        await Init(directory.Root);
        File.WriteAllText(Path.Combine(directory.Root, ".gitmodules"),
            "[submodule \"dependency\"]\npath = dependency with spaces\nurl = https://unused.invalid/dependency.git\nignore = all\n");
        var first_party = Path.Combine(directory.Root, "first-party.txt");
        File.WriteAllText(first_party, "original\n");
        await Git(directory.Root, "add", ".gitmodules", "first-party.txt");
        await Git(directory.Root, "update-index", "--add", "--cacheinfo", $"160000,{commit},dependency with spaces");
        await Git(directory.Root, "commit", "-m", "root");
        await Git(directory.Root, "submodule", "init");
        await Git(directory.Root, "config", "submodule.dependency.ignore", "all");
        File.WriteAllText(first_party, "allowed candidate edit\n");
        var source = await BenchmarkRunContext.SourceAsync(app, directory.Root, default);
        Assert.IsTrue(source.Dirty);
        await BenchmarkRunContext.VerifySourceAsync(app, source, default);
        var status = await Git(directory.Root, "status", "--porcelain");
        var diff = await Git(directory.Root, "diff", "HEAD", "--binary");
        var changed = change == "untracked" ? Path.Combine(module, "new.txt") : tracked;
        File.WriteAllText(changed, "dirty once\n");
        if (change == "staged") await Git(module, "add", "tracked.txt");
        await Rejected();
        File.WriteAllText(changed, "already dirty, different contents\n");
        await Rejected();

        async Task Rejected()
        {
            Assert.AreEqual(status, await Git(directory.Root, "status", "--porcelain"));
            Assert.AreEqual(diff, await Git(directory.Root, "diff", "HEAD", "--binary"));
            var error = await Assert.ThrowsExceptionAsync<BenchmarkToolException>(() => BenchmarkRunContext.SourceAsync(app, directory.Root, default));
            StringAssert.Contains(error.Message, "dirty initialized submodules");
            await Assert.ThrowsExceptionAsync<BenchmarkToolException>(() => BenchmarkRunContext.VerifySourceAsync(app, source, default));
        }
        async Task<string> Git(string root, params string[] args) => await BenchmarkGit.TextAsync(app, root, args, default);
        async Task Init(string root)
        {
            await Git(root, "init", "--initial-branch=fixture");
            await Git(root, "config", "user.name", "Benchmark test");
            await Git(root, "config", "user.email", "benchmark@example.invalid");
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
