using System.IO;
using System;
using UnrealBuildTool;

public class NativeS7 : ModuleRules
{
    public NativeS7(ReadOnlyTargetRules Target) : base(Target)
    {
        Type = ModuleType.External;
        PublicDependencyModuleNames.Add("SandboxCore");

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
        string nativeToolchain = Environment.GetEnvironmentVariable("IOJ_NATIVE_TOOLCHAIN") ?? "clang-cl";
        string includeDirectory = Path.Combine(repositoryRoot, "native", "s7", "runtime", "lib", "include");
        PublicSystemIncludePaths.Add(includeDirectory);
        PublicSystemIncludePaths.Add(Path.Combine(repositoryRoot, "native", "s7", "data", "lib", "include"));
        if (!Target.bGenerateProjectFiles)
        {
            string libraryPath = Path.Combine(
                repositoryRoot,
                "Binaries",
                "Native",
                "S7",
                nativeToolchain,
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
            string dataLibraryPath = Path.Combine(Path.GetDirectoryName(libraryPath)!, "native-s7-data.lib");
            PublicAdditionalLibraries.Add(dataLibraryPath);
            ExternalDependencies.Add(dataLibraryPath);
            ExternalDependencies.Add(libraryPath);
            // Supply the static archive dependency explicitly for Unreal's external-module linker.
            string coreLibraryPath = Path.Combine(repositoryRoot, "Binaries", "Native", "Core",
                nativeToolchain, Target.Platform.ToString(), Target.Configuration.ToString(), "SandboxNativeCore.lib");
            PublicAdditionalLibraries.Add(coreLibraryPath);
            ExternalDependencies.Add(coreLibraryPath);
        }
    }
}
