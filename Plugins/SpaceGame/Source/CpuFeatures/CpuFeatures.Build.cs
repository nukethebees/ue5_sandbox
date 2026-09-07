using UnrealBuildTool;
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
                "The repository provisions the x64-windows dynamic-CRT triplet.");
        }

        string repositoryRoot = Path.GetFullPath(Path.Combine(ModuleDirectory, "..", "..", "..", ".."));
        string packageRoot = Path.Combine(repositoryRoot, "vcpkg_installed", "x64-windows");
        bool useDebugLibrary = Target.Configuration == UnrealTargetConfiguration.Debug &&
                               Target.bDebugBuildsActuallyUseDebugCRT;
        string libraryDirectory = useDebugLibrary ? Path.Combine("debug", "lib") : "lib";
        string libraryPath = Path.Combine(packageRoot, libraryDirectory, "cpu_features.lib");
        if (!File.Exists(libraryPath))
        {
            throw new BuildException(
                "CpuFeatures expected the vcpkg artifact at '{0}'. " +
                "Configure an Unreal CMake preset to provision the repository install tree.",
                libraryPath);
        }

        PublicSystemIncludePaths.Add(Path.Combine(packageRoot, "include", "cpu_features"));
        PublicDefinitions.Add("STACK_LINE_READER_BUFFER_SIZE=1024");
        PublicAdditionalLibraries.Add(libraryPath);
    }
}
