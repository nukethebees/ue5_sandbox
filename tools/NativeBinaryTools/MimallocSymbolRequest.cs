namespace NativeBinaryTools;

internal enum MimallocSymbolCommand
{
    Generate,
    Verify,
}

internal sealed record MimallocSymbolRequest(
    MimallocSymbolCommand Command,
    string NmPath,
    string ReadObjPath,
    string? OutputPath,
    IReadOnlyList<string> ObjectPaths);
