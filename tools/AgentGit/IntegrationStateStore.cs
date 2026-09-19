using System.Text.Json;

namespace AgentGit;

internal sealed record ReviewReceipt(
    int Version,
    string PatchFingerprint,
    string Tree,
    string BaseCommit,
    string TipCommit,
    string FeatureBranch,
    DateTimeOffset ReviewedAt);

internal sealed record ValidationReceipt(
    int Version,
    string Gate,
    string InputFingerprint,
    string PatchFingerprint,
    string BaseCommit,
    string Tree,
    string Environment,
    DateTimeOffset CompletedAt);

internal sealed class IntegrationStateStore
{
    private static readonly JsonSerializerOptions json_options = new() { WriteIndented = true };
    private readonly string root;

    public IntegrationStateStore(string common_git_directory)
    {
        root = Path.Combine(common_git_directory, "agent-git", "integration");
    }

    public ReviewReceipt? ReadReview(string patch_fingerprint)
    {
        var receipt = Read<ReviewReceipt>(Path.Combine(root, "reviews", $"{patch_fingerprint}.json"));
        return receipt is { Version: 1 } &&
            string.Equals(receipt.PatchFingerprint, patch_fingerprint, StringComparison.Ordinal)
                ? receipt
                : null;
    }

    public void WriteReview(ReviewReceipt receipt)
    {
        WriteAtomic(Path.Combine(root, "reviews", $"{receipt.PatchFingerprint}.json"), receipt);
    }

    public ValidationReceipt? ReadValidation(string receipt_key)
    {
        var receipt = Read<ValidationReceipt>(Path.Combine(root, "validations", $"{receipt_key}.json"));
        return receipt is { Version: 1 } &&
            string.Equals(receipt.InputFingerprint, receipt_key, StringComparison.Ordinal)
                ? receipt
                : null;
    }

    public void WriteValidation(string receipt_key, ValidationReceipt receipt)
    {
        WriteAtomic(Path.Combine(root, "validations", $"{receipt_key}.json"), receipt);
    }

    public void AppendAudit(object entry)
    {
        Directory.CreateDirectory(root);
        var line = JsonSerializer.Serialize(entry) + Environment.NewLine;
        File.AppendAllText(Path.Combine(root, "audit.jsonl"), line);
    }

    private static T? Read<T>(string path)
    {
        return File.Exists(path) ? JsonSerializer.Deserialize<T>(File.ReadAllText(path)) : default;
    }

    private static void WriteAtomic<T>(string path, T value)
    {
        Directory.CreateDirectory(Path.GetDirectoryName(path)!);
        var temporary = $"{path}.{Guid.NewGuid():N}.tmp";
        File.WriteAllText(temporary, JsonSerializer.Serialize(value, json_options));
        File.Move(temporary, path, overwrite: true);
    }
}
