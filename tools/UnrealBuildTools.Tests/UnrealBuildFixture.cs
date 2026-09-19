using UnrealBuildTools;

namespace UnrealBuildTools.Tests;

internal sealed class UnrealBuildFixture : IDisposable
{
    private readonly string root;

    public UnrealBuildFixture()
    {
        root = Directory.CreateTempSubdirectory("SandboxUnrealBuildTools-").FullName;
        ProjectRoot = Path.Combine(root, "Project Root");
        ProjectPath = Path.Combine(ProjectRoot, "Sandbox.uproject");
        EngineRoot = Path.Combine(root, "Engine Root");
        BuildScriptPath = Path.Combine(EngineRoot, "Engine", "Build", "BatchFiles", "RunUBT.cmd");

        Directory.CreateDirectory(ProjectRoot);
        File.WriteAllText(ProjectPath, "{}");
        Directory.CreateDirectory(Path.GetDirectoryName(BuildScriptPath)!);
        WriteBuildScript(
            "@echo off\r\necho %* > \"%~dp0arguments.txt\"\r\nset > \"%~dp0environment.txt\"\r\nexit /b 0\r\n");
    }

    public string ProjectRoot { get; }

    public string ProjectPath { get; }

    public string EngineRoot { get; }

    public string BuildScriptPath { get; }

    public BuildRequest CreateRequest(bool verify_editor_modules)
    {
        return new BuildRequest(
            BuildScriptPath,
            "SandboxEditor",
            "Win64",
            "Development",
            ProjectPath,
            "fixture-toolchain",
            verify_editor_modules);
    }

    public string ProjectManifestPath(string configuration)
    {
        return Path.Combine(
            ProjectRoot,
            "Binaries",
            "Win64",
            EditorModuleManifestLocator.GetManifestName(configuration));
    }

    public void WriteReceipt(string target, string configuration, string contents)
    {
        var path = Path.Combine(
            ProjectRoot,
            "Binaries",
            "Win64",
            EditorModuleManifestLocator.GetTargetReceiptName(target, configuration));
        WriteFile(path, contents);
    }

    public void WriteEditorVersion(string build_id)
    {
        WriteEditorVersionJson($"{{ \"BuildId\": \"{build_id}\" }}");
    }

    public void WriteEditorVersionJson(string contents)
    {
        WriteFile(Path.Combine(EngineRoot, "Engine", "Binaries", "Win64", "UnrealEditor.version"), contents);
    }

    public void WriteManifest(string path, string build_id)
    {
        WriteFile(path, $"{{ \"BuildId\": \"{build_id}\" }}");
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

    private static void WriteFile(string path, string contents)
    {
        Directory.CreateDirectory(Path.GetDirectoryName(path)!);
        File.WriteAllText(path, contents);
    }
}
