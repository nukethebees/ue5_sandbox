namespace UnrealBuildTools;

public sealed class UnrealBuildOrchestrator
{
    private readonly EditorModuleManifestLocator manifest_locator = new();
    private readonly EditorModuleCompatibilityChecker compatibility_checker = new();
    private readonly BuildScriptRunner build_script_runner = new();

    public BuildOutcome Build(BuildRequest request, Action<string>? warning_sink = null)
    {
        ArgumentNullException.ThrowIfNull(request);

        var paths = BuildPaths.Resolve(request);
        var warnings = new List<string>();
        var force_rebuild = false;

        if (request.VerifyEditorModules)
        {
            var discovery = manifest_locator.Discover(paths.ProjectRoot, request.Target, request.Configuration);
            if (discovery.Problem is not null)
            {
                AddWarning($"{discovery.Problem} Forcing an editor rebuild.");
                force_rebuild = true;
            }
            else
            {
                var compatibility = compatibility_checker.Check(paths.EngineRoot, discovery.Paths);
                if (compatibility.Problems.Count > 0)
                {
                    AddWarning($"Editor module BuildId mismatch detected. Forcing '{request.Target} {request.Platform} {request.Configuration}' to resynchronize modules.");
                    foreach (var problem in compatibility.Problems)
                    {
                        AddWarning($"  {problem}");
                    }
                    force_rebuild = true;
                }
            }
        }

        build_script_runner.Run(paths, request, force_rebuild);

        if (request.VerifyEditorModules)
        {
            var discovery = manifest_locator.Discover(paths.ProjectRoot, request.Target, request.Configuration);
            if (discovery.Problem is not null)
            {
                throw new PostBuildCompatibilityException(discovery.Problem);
            }

            var compatibility = compatibility_checker.Check(paths.EngineRoot, discovery.Paths);
            if (compatibility.Problems.Count > 0)
            {
                throw new PostBuildCompatibilityException(
                    $"Editor module BuildIds remain incompatible after the forced rebuild:{Environment.NewLine}" +
                    string.Join(Environment.NewLine, compatibility.Problems));
            }
        }

        return new BuildOutcome(force_rebuild, warnings);

        void AddWarning(string warning)
        {
            warnings.Add(warning);
            warning_sink?.Invoke(warning);
        }
    }
}
