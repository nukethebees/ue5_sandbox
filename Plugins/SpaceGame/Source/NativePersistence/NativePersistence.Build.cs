using System;
using System.IO;
using UnrealBuildTool;

public class NativePersistence : ModuleRules
{
    public NativePersistence(ReadOnlyTargetRules Target) : base(Target)
    {
        Type = ModuleType.External;
        PublicDependencyModuleNames.Add("NativeLevels");
        PublicDependencyModuleNames.Add("NativeSimulation");
        PublicDependencyModuleNames.Add("NativeOneTBB");

        string repositoryRoot = Path.GetFullPath(Path.Combine(ModuleDirectory, "..", "..", "..", ".."));
        string nativeToolchain = Environment.GetEnvironmentVariable("IOJ_NATIVE_TOOLCHAIN") ?? "clang-cl";
        PublicSystemIncludePaths.Add(Path.Combine(repositoryRoot, "native", "persistence", "lib", "include"));
        if (!Target.bGenerateProjectFiles)
        {
            string libraryPath = Path.Combine(repositoryRoot, "Binaries", "Native", "Persistence", nativeToolchain,
                Target.Platform.ToString(), Target.Configuration.ToString(), "native-persistence.lib");
            if (!File.Exists(libraryPath))
            {
                throw new BuildException("NativePersistence expected '{0}'. Build through the repository CMake workflow.", libraryPath);
            }
            PublicAdditionalLibraries.Add(libraryPath);
            ExternalDependencies.Add(libraryPath);
        }
    }
}
