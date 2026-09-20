namespace ArchitectureChecks;

public sealed record ModuleMigrationRequest(
    string RootPath,
    string Baseline,
    string OldModule,
    IReadOnlyList<string> PluginModules);
