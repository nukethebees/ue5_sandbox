using UnrealBuildTool;
using System;
using System.IO;

public class CpuFeatures : ModuleRules
{
    public CpuFeatures(ReadOnlyTargetRules Target) : base(Target)
    {
        Type = ModuleType.External;

        bool supported = Target.Platform == UnrealTargetPlatform.Win64 &&
                         Target.Architecture == UnrealArch.X64;
        PublicDefinitions.Add("WITH_CPU_FEATURES=" + (supported ? "1" : "0"));
        if (!supported)
        {
            return;
        }

        if (Target.bUseStaticCRT)
        {
            throw new BuildException(
                "CpuFeatures does not support Unreal targets that use the static CRT. " +
                "The repository CMake target uses the dynamic release CRT.");
        }

        string repositoryRoot = Path.GetFullPath(Path.Combine(ModuleDirectory, "..", "..", "..", ".."));
        string nativeToolchain = Environment.GetEnvironmentVariable("SANDBOX_NATIVE_TOOLCHAIN") ?? "clang-cl";
        PublicSystemIncludePaths.Add(
            Path.Combine(repositoryRoot, "native", "third_party", "cpu_features", "include"));
        PublicDefinitions.Add("STACK_LINE_READER_BUFFER_SIZE=1024");

        if (!Target.bGenerateProjectFiles)
        {
            string libraryPath = Path.Combine(
                repositoryRoot,
                "Binaries",
                "Native",
                "CpuFeatures",
                nativeToolchain,
                Target.Platform.ToString(),
                Target.Configuration.ToString(),
                "cpu_features.lib");
            if (!File.Exists(libraryPath))
            {
                throw new BuildException(
                    "CpuFeatures expected the CMake-built library at '{0}'. " +
                    "Build Unreal targets through a repository CMake workflow.",
                    libraryPath);
            }

            PublicAdditionalLibraries.Add(libraryPath);
            ExternalDependencies.Add(libraryPath);
        }
    }
}
