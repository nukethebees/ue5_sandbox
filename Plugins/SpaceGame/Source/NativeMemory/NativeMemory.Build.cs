using UnrealBuildTool;
using System.IO;

public class NativeMemory : ModuleRules
{
    public NativeMemory(ReadOnlyTargetRules Target) : base(Target)
    {
        Type = ModuleType.External;

        if (Target.Platform != UnrealTargetPlatform.Win64 ||
            Target.Architecture != UnrealArch.X64)
        {
            throw new BuildException("NativeMemory currently supports only Win64 x64 targets.");
        }

        if (Target.bUseStaticCRT ||
            (Target.Configuration == UnrealTargetConfiguration.Debug &&
             Target.bDebugBuildsActuallyUseDebugCRT))
        {
            throw new BuildException(
                "NativeMemory requires the dynamic release CRT used by the CMake target.");
        }

        string repositoryRoot = Path.GetFullPath(
            Path.Combine(ModuleDirectory, "..", "..", "..", ".."));
        string includeDirectory = Path.Combine(repositoryRoot, "native", "memory", "include");
        PublicSystemIncludePaths.Add(includeDirectory);
        if (!Target.bGenerateProjectFiles)
        {
            string libraryPath = Path.Combine(
                repositoryRoot,
                "Binaries",
                "NativeMemory",
                Target.Platform.ToString(),
                Target.Configuration.ToString(),
                "SandboxNativeMemory.lib");
            if (!File.Exists(libraryPath))
            {
                throw new BuildException(
                    "NativeMemory expected the CMake-built library at '{0}'. " +
                    "Build Unreal targets through a repository CMake workflow.",
                    libraryPath);
            }
            PublicAdditionalLibraries.Add(libraryPath);
            ExternalDependencies.Add(libraryPath);
        }
    }
}
