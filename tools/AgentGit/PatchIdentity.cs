using System.Security.Cryptography;
using System.Text;

namespace AgentGit;

internal sealed record PatchIdentity(string Fingerprint, string Tree, IReadOnlyList<string> ChangedPaths);

internal sealed class PatchIdentityService(GitClient git)
{
    private const string format = "agent-git-patch-v1";

    public async Task<PatchIdentity> ComputeAsync(
        string worktree,
        string base_commit,
        string tip_commit,
        CancellationToken cancellation_token)
    {
        var diff = await git.RunAsync(
            worktree,
            [
                "diff", "--binary", "--full-index", "--no-renames", "--no-ext-diff", "--no-textconv",
                "--unified=0", base_commit, tip_commit, "--",
            ],
            cancellation_token: cancellation_token);
        GitClient.EnsureSuccess(diff, ["diff"]);

        var canonical = Canonicalize(diff.StandardOutput);
        var fingerprint = Convert.ToHexString(SHA256.HashData(canonical)).ToLowerInvariant();
        var tree = await git.RequireTextAsync(
            worktree,
            ["rev-parse", $"{tip_commit}^{{tree}}"],
            cancellation_token);
        var paths = await git.RequireTextAsync(
            worktree,
            ["diff", "--name-only", "--no-renames", base_commit, tip_commit, "--"],
            cancellation_token);
        return new PatchIdentity(
            fingerprint,
            tree,
            paths.Split('\n', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries));
    }

    internal static byte[] Canonicalize(byte[] diff)
    {
        var text = Encoding.UTF8.GetString(diff).Replace("\r\n", "\n", StringComparison.Ordinal);
        var result = new StringBuilder(format).Append('\n');
        foreach (var line in text.Split('\n'))
        {
            if (line.StartsWith("index ", StringComparison.Ordinal))
            {
                continue;
            }
            if (line.StartsWith("@@ ", StringComparison.Ordinal))
            {
                result.Append("@@\n");
                continue;
            }

            result.Append(line).Append('\n');
        }

        return Encoding.UTF8.GetBytes(result.ToString());
    }
}
