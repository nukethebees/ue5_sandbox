namespace UnrealBuildTools;

public sealed record BuildPaths(string BuildScriptPath, string ProjectPath, string ProjectRoot, string EngineRoot)
{
    public static BuildPaths Resolve(BuildRequest request)
    {
        ArgumentNullException.ThrowIfNull(request);

        var build_script_path = ResolveFile(request.BuildScriptPath, "build script");
        var project_path = ResolveFile(request.ProjectPath, "project");
        var project_root = Directory.GetParent(project_path)?.FullName;
        if (string.IsNullOrWhiteSpace(project_root))
        {
            throw new ToolInputException($"Unable to determine the project root from '{project_path}'.");
        }

        var batch_files_directory = Directory.GetParent(build_script_path);
        var build_directory = batch_files_directory?.Parent;
        var engine_directory = build_directory?.Parent;
        var engine_root = engine_directory?.Parent?.FullName;
        if (string.IsNullOrWhiteSpace(engine_root))
        {
            throw new ToolInputException(
                $"Unable to determine the engine root from build script '{build_script_path}'. " +
                "Expected an Engine/Build/BatchFiles script path.");
        }

        return new BuildPaths(build_script_path, project_path, project_root, engine_root);
    }

    private static string ResolveFile(string path, string description)
    {
        if (string.IsNullOrWhiteSpace(path))
        {
            throw new ToolInputException($"The {description} path must not be empty.");
        }

        var full_path = Path.GetFullPath(path);
        if (!File.Exists(full_path))
        {
            throw new ToolInputException($"The {description} file is missing: '{full_path}'.");
        }

        return full_path;
    }
}
