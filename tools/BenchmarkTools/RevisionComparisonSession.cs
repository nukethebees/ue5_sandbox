namespace BenchmarkTools;

internal static class BenchmarkGit
{
    public static ProcessRequest Request(string root, IReadOnlyList<string> arguments)
    {
        var safe = new List<string>
        {
            "--no-pager", "--literal-pathspecs", "-c", "core.protectNTFS=true", "-c", "core.protectHFS=true",
            "-c", "core.hooksPath=NUL", "-c", "gc.auto=0", "-c", "maintenance.auto=false", "-c", "submodule.recurse=false",
        };
        safe.AddRange(arguments);
        return new ProcessRequest("git", safe, root, new Dictionary<string, string?>
        {
            ["GIT_TERMINAL_PROMPT"] = "0", ["GCM_INTERACTIVE"] = "Never",
            ["GIT_EDITOR"] = "false", ["GIT_SEQUENCE_EDITOR"] = "false",
        }, TimeSpan.FromMinutes(30));
    }

    public static async Task<string> TextAsync(BenchmarkToolsApplication application, string root, IReadOnlyList<string> arguments, CancellationToken token)
    {
        var result = await application.ProcessRunner.RunAsync(Request(root, arguments), token);
        if (result.ExitCode != 0) throw new BenchmarkToolException($"Git {arguments[0]} failed: {result.StandardError.Trim()}");
        return result.StandardOutput.Trim();
    }

    public static async Task SuccessAsync(BenchmarkToolsApplication application, ProcessRequest request, CancellationToken token)
    {
        var result = await application.ProcessRunner.RunAsync(request, token);
        application.WriteProcessOutput(result);
        if (result.ExitCode != 0) throw new BenchmarkToolException($"'{request.FileName}' failed with exit code {result.ExitCode}: {result.StandardError.Trim()}");
    }
}

internal sealed class RevisionComparisonSession : IAsyncDisposable
{
    private readonly BenchmarkToolsApplication application_;
    private readonly string parent_;
    private readonly bool keep_;
    private bool owned_;

    private RevisionComparisonSession(BenchmarkToolsApplication application, RevisionIdentity candidate, string baseline_root, string parent, bool keep)
    {
        application_ = application;
        Candidate = candidate;
        BaselineRoot = baseline_root;
        parent_ = parent;
        keep_ = keep;
    }

    public RevisionIdentity Candidate { get; }
    public RevisionIdentity Baseline { get; private set; } = null!;
    public string BaselineRoot { get; }
    public bool OwnsBaseline => owned_;

    public static async Task<RevisionComparisonSession> CreateAsync(BenchmarkToolsApplication application, RepositoryPaths repository,
        string baseline, string run_id, string? supplied, bool keep, CancellationToken token)
    {
        var candidate = await BenchmarkRunContext.SourceAsync(application, repository.Root, token);
        var commit = await BenchmarkGit.TextAsync(application, repository.Root, ["rev-parse", "--verify", "--end-of-options", baseline + "^{commit}"], token);
        var parent = Path.Combine(repository.Root, ".local", "benchmarks", "worktrees");
        var path = supplied is null ? Path.Combine(parent, run_id, "baseline") : Path.GetFullPath(supplied, repository.Root);
        var session = new RevisionComparisonSession(application, candidate, path, parent, keep);
        try
        {
            if (supplied is null)
            {
                ValidateOwnedPath(path, parent);
                if (Directory.Exists(path) || File.Exists(path)) throw new BenchmarkToolException($"Benchmark worktree path already exists: '{path}'.");
                Directory.CreateDirectory(Path.GetDirectoryName(path)!);
                // Ownership is established before Git starts, so a partially failed add is cleaned up too.
                session.owned_ = true;
                await BenchmarkGit.TextAsync(application, repository.Root, ["worktree", "add", "--detach", path, commit], token);
            }
            else
            {
                if (!Directory.Exists(path)) throw new BenchmarkToolException($"Prepared baseline worktree does not exist: '{path}'.");
                var top = await BenchmarkGit.TextAsync(application, path, ["rev-parse", "--show-toplevel"], token);
                if (!string.Equals(Path.GetFullPath(top), path, StringComparison.OrdinalIgnoreCase))
                    throw new BenchmarkToolException("Prepared baseline must be a worktree root.");
            }
            session.Baseline = await BenchmarkRunContext.SourceAsync(application, path, token);
            if (session.Baseline.Commit != commit) throw new BenchmarkToolException($"Prepared worktree is not at baseline commit {commit}: '{path}'.");
            if (session.Baseline.Dirty) throw new BenchmarkToolException("Baseline worktree must be clean before harness preparation.");
            return session;
        }
        catch
        {
            await session.DisposeAsync();
            throw;
        }
    }

    internal static void ValidateOwnedPath(string path, string parent)
    {
        var full = Path.GetFullPath(path);
        var relative = Path.GetRelativePath(Path.GetFullPath(parent), full);
        if (Path.IsPathRooted(relative) || relative == "." || relative == ".." || relative.StartsWith(".." + Path.DirectorySeparatorChar, StringComparison.Ordinal))
            throw new BenchmarkToolException($"Refusing benchmark worktree outside '{parent}'.");
        for (var directory = new DirectoryInfo(full); directory is not null; directory = directory.Parent)
        {
            if (directory.Exists && (directory.Attributes & FileAttributes.ReparsePoint) != 0)
                throw new BenchmarkToolException($"Benchmark worktree path traverses a link: '{directory.FullName}'.");
        }
    }

    public async ValueTask DisposeAsync()
    {
        if (!owned_ || keep_) return;
        ValidateOwnedPath(BaselineRoot, parent_);
        // A rejected add can leave no registration at all. Do not mask that error with remove.
        if (!Directory.Exists(BaselineRoot) && !File.Exists(BaselineRoot)) return;
        var result = await application_.ProcessRunner.RunAsync(BenchmarkGit.Request(Candidate.Root, ["worktree", "remove", "--force", "--force", BaselineRoot]), CancellationToken.None);
        if (result.ExitCode != 0)
            throw new BenchmarkToolException($"Could not clean owned benchmark worktree '{BaselineRoot}': {result.StandardError.Trim()}");
        owned_ = false;
    }
}

internal sealed record BenchmarkRepetition(int Sequence, int Repetition, string Side, bool Warmup);

internal static class BenchmarkOrdering
{
    public static IReadOnlyList<BenchmarkRepetition> Balanced(int repetitions, int warmups = 0)
    {
        var result = new List<BenchmarkRepetition>();
        foreach (var warmup in new[] { true, false })
        {
            var count = warmup ? warmups : repetitions;
            for (var repetition = 1; repetition <= count; ++repetition)
                foreach (var side in repetition % 2 == 1 ? new[] { "baseline", "candidate" } : new[] { "candidate", "baseline" })
                    result.Add(new BenchmarkRepetition(result.Count + 1, repetition, side, warmup));
        }
        return result;
    }
}
