using System;
using System.IO;
using UnrealBuildTool;

public class NativePresentation : ModuleRules
{
    public NativePresentation(ReadOnlyTargetRules Target) : base(Target)
    {
        Type = ModuleType.External;
        PublicDependencyModuleNames.AddRange(new string[] { "NativeSimulation", "NativeUI" });
        string repositoryRoot = Path.GetFullPath(Path.Combine(ModuleDirectory, "..", "..", "..", ".."));
        string nativeToolchain = Environment.GetEnvironmentVariable("IOJ_NATIVE_TOOLCHAIN") ?? "clang-cl";
        PublicSystemIncludePaths.Add(Path.Combine(repositoryRoot, "native", "presentation", "lib", "include"));
        if (!Target.bGenerateProjectFiles)
        {
            string libraryPath = Path.Combine(repositoryRoot, "Binaries", "Native", "Presentation", nativeToolchain,
                Target.Platform.ToString(), Target.Configuration.ToString(), "native-presentation.lib");
            if (!File.Exists(libraryPath))
            {
                throw new BuildException("NativePresentation expected '{0}'. Build through the repository CMake workflow.", libraryPath);
            }
            PublicAdditionalLibraries.Add(libraryPath);
            ExternalDependencies.Add(libraryPath);
        }
    }
}
