using System;
using System.IO;
using UnrealBuildTool;

public class NativeProfiles : ModuleRules
{
    public NativeProfiles(ReadOnlyTargetRules Target) : base(Target)
    {
        Type = ModuleType.External;
        PublicDependencyModuleNames.Add("NativeLevels");
        PublicDependencyModuleNames.Add("NativeSimulation");
        PublicDependencyModuleNames.Add("NativeOneTBB");

        string repositoryRoot = Path.GetFullPath(Path.Combine(ModuleDirectory, "..", "..", "..", ".."));
        string nativeToolchain = Environment.GetEnvironmentVariable("IOJ_NATIVE_TOOLCHAIN") ?? "clang-cl";
        PublicSystemIncludePaths.Add(Path.Combine(repositoryRoot, "native", "profiles", "lib", "include"));
        if (!Target.bGenerateProjectFiles)
        {
            string libraryPath = Path.Combine(repositoryRoot, "Binaries", "Native", "Profiles", nativeToolchain,
                Target.Platform.ToString(), Target.Configuration.ToString(), "native-profiles.lib");
            if (!File.Exists(libraryPath))
            {
                throw new BuildException("NativeProfiles expected '{0}'. Build through the repository CMake workflow.", libraryPath);
            }
            PublicAdditionalLibraries.Add(libraryPath);
            ExternalDependencies.Add(libraryPath);
        }
    }
}
