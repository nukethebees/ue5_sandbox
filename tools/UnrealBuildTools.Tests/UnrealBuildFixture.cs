using UnrealBuildTools;

namespace UnrealBuildTools.Tests;

internal sealed class UnrealBuildFixture : IDisposable
{
    private readonly string root;

    public UnrealBuildFixture()
    {
        root = Directory.CreateTempSubdirectory("SandboxUnrealBuildTools-").FullName;
        var project_root = Path.Combine(root, "Project Root");
        ProjectPath = Path.Combine(project_root, "Sandbox.uproject");
        var engine_root = Path.Combine(root, "Engine Root");
        BuildScriptPath = Path.Combine(engine_root, "Engine", "Build", "BatchFiles", "RunUBT.cmd");

        Directory.CreateDirectory(project_root);
        File.WriteAllText(ProjectPath, "{}");
        Directory.CreateDirectory(Path.GetDirectoryName(BuildScriptPath)!);
        WriteBuildScript(
            "@echo off\r\necho %* > \"%~dp0arguments.txt\"\r\nset > \"%~dp0environment.txt\"\r\nexit /b 0\r\n");
    }

    public string ProjectPath { get; }

    public string BuildScriptPath { get; }

    public BuildRequest CreateRequest()
    {
        return new BuildRequest(
            BuildScriptPath,
            "SandboxEditor",
            "Win64",
            "Development",
            ProjectPath,
            "fixture-toolchain");
    }

    public string[] CreateArguments()
    {
        return
        [
            "--build-script", BuildScriptPath,
            "--target", "SandboxEditor",
            "--platform", "Win64",
            "--configuration", "Development",
            "--project", ProjectPath,
            "--native-toolchain", "fixture-toolchain",
        ];
    }

    public void WriteBuildScript(string contents)
    {
        File.WriteAllText(BuildScriptPath, contents);
    }

    public string ReadBuildArguments()
    {
        return File.ReadAllText(Path.Combine(Path.GetDirectoryName(BuildScriptPath)!, "arguments.txt"));
    }

    public string ReadBuildEnvironment()
    {
        return File.ReadAllText(Path.Combine(Path.GetDirectoryName(BuildScriptPath)!, "environment.txt"));
    }

    public void Dispose()
    {
        Directory.Delete(root, true);
    }
}
