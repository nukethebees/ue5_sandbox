namespace UnrealBuildTools;

public sealed record BuildPaths(string BuildScriptPath, string ProjectPath)
{
    public static BuildPaths Resolve(BuildRequest request)
    {
        ArgumentNullException.ThrowIfNull(request);

        var build_script_path = ResolveFile(request.BuildScriptPath, "build script");
        var project_path = ResolveFile(request.ProjectPath, "project");
        return new BuildPaths(build_script_path, project_path);
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
