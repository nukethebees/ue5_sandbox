using System.Security.Cryptography;
using System.Text;

namespace BenchmarkTools;

internal sealed record RevisionIdentity(string Root, string Commit, bool Dirty, string Status, string DiffSha256);

internal sealed class BenchmarkManifest
{
    public int SchemaVersion { get; init; } = 1;
    public int ToolSchemaVersion { get; init; } = 1;
    public required string RunId { get; init; }
    public required string Benchmark { get; init; }
    public DateTimeOffset CreatedUtc { get; init; } = DateTimeOffset.UtcNow;
    public string Status { get; set; } = "preparing";
    public object? Configuration { get; set; }
    public object? Provenance { get; set; }
    public object? Comparability { get; set; }
    public Dictionary<string, string> Artifacts { get; init; } = [];
    public List<string> ExpectedArtifacts { get; init; } = [];
    public string? Failure { get; set; }
}

internal sealed class BenchmarkRunContext
{
    private BenchmarkRunContext(string directory, BenchmarkManifest manifest)
    {
        DirectoryPath = directory;
        Manifest = manifest;
    }

    public string DirectoryPath { get; }
    public BenchmarkManifest Manifest { get; }
    public string Artifact(string name) => Path.Combine(DirectoryPath, name);

    public static BenchmarkRunContext Create(RepositoryPaths repository, string benchmark, string? output = null, object? configuration = null)
    {
        var id = $"{DateTimeOffset.UtcNow:yyyyMMddTHHmmssfffZ}-{Guid.NewGuid():N}";
        var parent = BenchmarkCommandSupport.ResolveOutputDirectory(repository, output ?? $".local/benchmarks/{benchmark}");
        var directory = Path.Combine(parent, id);
        Directory.CreateDirectory(directory);
        var run = new BenchmarkRunContext(directory, new BenchmarkManifest { RunId = id, Benchmark = benchmark, Configuration = configuration });
        run.Publish();
        return run;
    }

    public void Expect(string name)
    {
        Manifest.Artifacts[name] = Artifact(name);
        Manifest.ExpectedArtifacts.Add(name);
        Publish();
    }

    public void ValidateArtifacts()
    {
        foreach (var name in Manifest.ExpectedArtifacts)
        {
            var path = Manifest.Artifacts[name];
            if (!File.Exists(path) || new FileInfo(path).Length == 0)
                throw new BenchmarkToolException($"Expected artifact was not produced or is empty: '{path}'.");
        }
    }

    public void Publish() => BenchmarkCommandSupport.WriteJson(Artifact("manifest.json"), Manifest);
    public void Complete() { Manifest.Status = "complete"; Publish(); }
    public void Fail(Exception error) { Manifest.Status = "failed"; Manifest.Failure = error.Message; Publish(); }

    public static async Task<RevisionIdentity> SourceAsync(BenchmarkToolsApplication application, string root, CancellationToken token)
    {
        var commit = await BenchmarkGit.TextAsync(application, root, ["rev-parse", "--verify", "HEAD^{commit}"], token);
        var status = await BenchmarkGit.TextAsync(application, root, ["status", "--porcelain=v1", "--untracked-files=all"], token);
        var diff = await BenchmarkGit.TextAsync(application, root, ["diff", "HEAD", "--binary", "--no-ext-diff", "--no-textconv"], token);
        return new RevisionIdentity(root, commit, status.Length != 0, status,
            Convert.ToHexString(SHA256.HashData(Encoding.UTF8.GetBytes(diff))).ToLowerInvariant());
    }
}
