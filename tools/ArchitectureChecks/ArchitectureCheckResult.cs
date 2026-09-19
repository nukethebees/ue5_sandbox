namespace ArchitectureChecks;

public sealed record ArchitectureCheckResult(IReadOnlyList<string> Diagnostics)
{
    public bool IsValid => Diagnostics.Count == 0;
}
