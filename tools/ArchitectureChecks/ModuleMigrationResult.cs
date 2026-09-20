namespace ArchitectureChecks;

public enum ReflectedTypeKind
{
    Class,
    Struct,
    Enum,
}

public sealed record ReflectedType(ReflectedTypeKind Kind, string CppName)
{
    public string ReflectionName => Kind switch
    {
        ReflectedTypeKind.Class when CppName.StartsWith('A') || CppName.StartsWith('U') => CppName[1..],
        ReflectedTypeKind.Struct when CppName.StartsWith('F') => CppName[1..],
        _ => CppName,
    };
}

public sealed record MovedReflectedTypeFinding(
    ReflectedType Type,
    string SourcePath,
    string DestinationPath,
    string DestinationModule,
    string OldObjectPath,
    string NewObjectPath,
    bool HasRedirect);

public sealed record OldModuleReferenceFinding(string Path);

public sealed record StaleIncludeFinding(
    string Path,
    int Line,
    string IncludePath,
    IReadOnlyList<string> CounterpartModules);

public sealed record RelativeIncludeFinding(string Path, int Line, string IncludeText);

public sealed record ModuleMigrationResult(
    string RepositoryRoot,
    string Baseline,
    int RenamedFileCount,
    IReadOnlyList<MovedReflectedTypeFinding> MovedTypes,
    IReadOnlyList<OldModuleReferenceFinding> OldModuleReferences,
    IReadOnlyList<StaleIncludeFinding> StaleIncludes,
    IReadOnlyList<RelativeIncludeFinding> RelativeIncludes);
