using System;
using System.IO;
using UnrealBuildTool;

public class NativeLevelAuthoring : ModuleRules
{
    public NativeLevelAuthoring(ReadOnlyTargetRules Target) : base(Target)
    {
        Type = ModuleType.External;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "NativeS7",
            "SandboxNative",
        });

        if (Target.Platform != UnrealTargetPlatform.Win64 ||
            Target.Architecture != UnrealArch.X64)
        {
            throw new BuildException("NativeLevelAuthoring currently supports only Win64 x64 targets.");
        }

        if (Target.bUseStaticCRT ||
            (Target.Configuration == UnrealTargetConfiguration.Debug &&
             Target.bDebugBuildsActuallyUseDebugCRT))
        {
            throw new BuildException(
                "NativeLevelAuthoring requires the dynamic release CRT used by the CMake target.");
        }

        string repositoryRoot = Path.GetFullPath(
            Path.Combine(ModuleDirectory, "..", "..", "..", ".."));
        string nativeToolchain =
            Environment.GetEnvironmentVariable("SANDBOX_NATIVE_TOOLCHAIN") ?? "clang-cl";
        PublicSystemIncludePaths.Add(
            Path.Combine(repositoryRoot, "native", "level_authoring", "lib", "include"));

        if (!Target.bGenerateProjectFiles)
        {
            string libraryPath = Path.Combine(
                repositoryRoot,
                "Binaries",
                "Native",
                "LevelAuthoring",
                nativeToolchain,
                Target.Platform.ToString(),
                Target.Configuration.ToString(),
                "SandboxNativeLevelAuthoring.lib");
            if (!File.Exists(libraryPath))
            {
                throw new BuildException(
                    "NativeLevelAuthoring expected the CMake-built library at '{0}'. " +
                    "Build Unreal targets through a repository CMake workflow.",
                    libraryPath);
            }
            PublicAdditionalLibraries.Add(libraryPath);
            ExternalDependencies.Add(libraryPath);
        }
    }
}
