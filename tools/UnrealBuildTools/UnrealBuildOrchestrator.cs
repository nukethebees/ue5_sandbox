namespace UnrealBuildTools;

public sealed class UnrealBuildOrchestrator
{
    private readonly BuildScriptRunner build_script_runner = new();

    public void Build(BuildRequest request)
    {
        ArgumentNullException.ThrowIfNull(request);

        var paths = BuildPaths.Resolve(request);
        build_script_runner.Run(paths, request);
    }
}
