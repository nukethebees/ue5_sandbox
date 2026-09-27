using System.Reflection;
using System.Security.Cryptography;

namespace BenchmarkTools;

internal static class SandboxIsmcCompatibility
{
    public static async Task ApplyAsync(BenchmarkToolsApplication application, RevisionComparisonSession session, string compatibility,
        BenchmarkRunContext run, CancellationToken token)
    {
        if (compatibility == "none") return;
        if (compatibility != "sandbox-ismc-v1") throw new BenchmarkToolException("Supported compatibility values: none, sandbox-ismc-v1.");
        if (!session.OwnsBaseline) throw new BenchmarkToolException("Harness compatibility requires an owned detached baseline; supplied worktrees are never patched.");
        using var resource = Assembly.GetExecutingAssembly().GetManifestResourceStream("BenchmarkTools.SandboxIsmcHarnessV1.patch")
            ?? throw new BenchmarkToolException("Missing packaged SandboxISMC harness compatibility patch.");
        using var bytes = new MemoryStream();
        await resource.CopyToAsync(bytes, token);
        var content = bytes.ToArray();
        var path = run.Artifact("harness-compatibility.patch");
        await File.WriteAllBytesAsync(path, content, token);
        BenchmarkCommandSupport.WriteJson(run.Artifact("compatibility.json"), new
        {
            Id = compatibility, Baseline = session.Baseline.Commit, PatchSha256 = Convert.ToHexString(SHA256.HashData(content)),
            Scope = "SandboxISMCLab output, viewport and result metadata only; no renderer implementation or workload changes",
        });
        run.Manifest.Artifacts["compatibility.json"] = run.Artifact("compatibility.json");
        run.Manifest.Artifacts["harness-compatibility.patch"] = path;
        run.Publish();
        await BenchmarkGit.TextAsync(application, session.BaselineRoot, ["apply", "--check", "--", path], token);
        await BenchmarkGit.TextAsync(application, session.BaselineRoot, ["apply", "--", path], token);
    }
}
