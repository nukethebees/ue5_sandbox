using System;
using System.IO;
using UnrealBuildTool;

public class NativeUI : ModuleRules
{
    public NativeUI(ReadOnlyTargetRules Target) : base(Target)
    {
        Type = ModuleType.External;
        string repositoryRoot = Path.GetFullPath(Path.Combine(ModuleDirectory, "..", "..", "..", ".."));
        string nativeToolchain = Environment.GetEnvironmentVariable("IOJ_NATIVE_TOOLCHAIN") ?? "clang-cl";
        PublicSystemIncludePaths.Add(Path.Combine(repositoryRoot, "native", "ui", "lib", "include"));
        if (!Target.bGenerateProjectFiles)
        {
            string libraryPath = Path.Combine(repositoryRoot, "Binaries", "Native", "UI", nativeToolchain,
                Target.Platform.ToString(), Target.Configuration.ToString(), "native-ui.lib");
            if (!File.Exists(libraryPath))
            {
                throw new BuildException("NativeUI expected '{0}'. Build through the repository CMake workflow.", libraryPath);
            }
            PublicAdditionalLibraries.Add(libraryPath);
            ExternalDependencies.Add(libraryPath);
        }
    }
}
