using System.Security.Cryptography;
using System.Text;

namespace BenchmarkTools;

internal sealed record RevisionIdentity(string Root, string Commit, bool Dirty, string Status, string DiffSha256,
    string UntrackedSha256, string? ArtifactRoot);

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

    public static async Task<RevisionIdentity> SourceAsync(BenchmarkToolsApplication application, string root, CancellationToken token, string? artifact_root = null)
    {
        var commit = await BenchmarkGit.TextAsync(application, root, ["rev-parse", "--verify", "HEAD^{commit}"], token);
        var status = await BenchmarkGit.TextAsync(application, root, ["status", "--porcelain=v1", "--untracked-files=no"], token);
        var diff = await BenchmarkGit.OutputAsync(application, root, ["diff", "HEAD", "--binary", "--no-ext-diff", "--no-textconv"], token);
        var untracked = await BenchmarkGit.OutputAsync(application, root, ["ls-files", "--others", "--exclude-standard", "-z"], token);
        using var content = IncrementalHash.CreateHash(HashAlgorithmName.SHA256);
        foreach (var path in untracked.Split('\0', StringSplitOptions.RemoveEmptyEntries).Order(StringComparer.Ordinal))
        {
            var full = Path.GetFullPath(path, root);
            if (artifact_root is not null && IsWithin(full, artifact_root)) continue;
            // Do not follow an untracked link outside the source checkout.
            var link = new FileInfo(full).LinkTarget;
            var digest = link is null ? await HashFileAsync(full, token) : SHA256.HashData(Encoding.UTF8.GetBytes(link));
            content.AppendData(Encoding.UTF8.GetBytes(path + '\0'));
            content.AppendData(digest);
            status += "\n?? " + path;
        }
        return new RevisionIdentity(root, commit, status.Length != 0, status,
            Convert.ToHexString(SHA256.HashData(Encoding.UTF8.GetBytes(diff))).ToLowerInvariant(),
            Convert.ToHexString(content.GetHashAndReset()).ToLowerInvariant(), artifact_root);
    }

    public static async Task VerifySourceAsync(BenchmarkToolsApplication application, RevisionIdentity expected, CancellationToken token)
    {
        var actual = await SourceAsync(application, expected.Root, token, expected.ArtifactRoot);
        if (expected.Commit != actual.Commit || expected.DiffSha256 != actual.DiffSha256 ||
            expected.Status != actual.Status || expected.UntrackedSha256 != actual.UntrackedSha256)
            throw new BenchmarkToolException($"Source changed during benchmark preparation or measurement: '{expected.Root}'. Results cannot be compared; start a new run.");
    }

    private static bool IsWithin(string path, string directory)
    {
        var relative = Path.GetRelativePath(directory, path);
        return !Path.IsPathRooted(relative) && relative != ".." && !relative.StartsWith(".." + Path.DirectorySeparatorChar, StringComparison.Ordinal);
    }

    private static async Task<byte[]> HashFileAsync(string path, CancellationToken token)
    {
        using var stream = File.OpenRead(path);
        return await SHA256.HashDataAsync(stream, token);
    }
}
