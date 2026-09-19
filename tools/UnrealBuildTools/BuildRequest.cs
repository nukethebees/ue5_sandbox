namespace UnrealBuildTools;

public sealed record BuildRequest(
    string BuildScriptPath,
    string Target,
    string Platform,
    string Configuration,
    string ProjectPath,
    string NativeToolchain);
