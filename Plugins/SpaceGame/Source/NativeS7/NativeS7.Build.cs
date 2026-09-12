using System.IO;
using UnrealBuildTool;

public class NativeS7 : ModuleRules
{
    public NativeS7(ReadOnlyTargetRules Target) : base(Target)
    {
        Type = ModuleType.External;

        if (Target.Platform != UnrealTargetPlatform.Win64 ||
            Target.Architecture != UnrealArch.X64)
        {
            throw new BuildException("NativeS7 currently supports only Win64 x64 targets.");
        }

        if (Target.bUseStaticCRT ||
            (Target.Configuration == UnrealTargetConfiguration.Debug &&
             Target.bDebugBuildsActuallyUseDebugCRT))
        {
            throw new BuildException(
                "NativeS7 requires the dynamic release CRT used by the CMake target.");
        }

        string repositoryRoot = Path.GetFullPath(
            Path.Combine(ModuleDirectory, "..", "..", "..", ".."));
        string includeDirectory = Path.Combine(repositoryRoot, "native", "s7", "lib", "include");
        PublicSystemIncludePaths.Add(includeDirectory);
        if (!Target.bGenerateProjectFiles)
        {
            string libraryPath = Path.Combine(
                repositoryRoot,
                "Binaries",
                "Native",
                "S7",
                Target.Platform.ToString(),
                Target.Configuration.ToString(),
                "sandbox_s7.lib");
            if (!File.Exists(libraryPath))
            {
                throw new BuildException(
                    "NativeS7 expected the CMake-built library at '{0}'. " +
                    "Build Unreal targets through a repository CMake workflow.",
                    libraryPath);
            }
            PublicAdditionalLibraries.Add(libraryPath);
            ExternalDependencies.Add(libraryPath);
        }
    }
}
