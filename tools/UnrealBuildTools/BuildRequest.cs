namespace UnrealBuildTools;

public sealed record BuildRequest(
    string BuildScriptPath,
    string Target,
    string Platform,
    string Configuration,
    string ProjectPath,
    string NativeToolchain,
    bool VerifyEditorModules);

public sealed record BuildOutcome(bool ForceRebuild, IReadOnlyList<string> Warnings);
