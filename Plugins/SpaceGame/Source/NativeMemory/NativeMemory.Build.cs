using UnrealBuildTool;
using System;
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
        string nativeToolchain = Environment.GetEnvironmentVariable("SANDBOX_NATIVE_TOOLCHAIN") ?? "clang-cl";
        string includeDirectory = Path.Combine(repositoryRoot, "native", "memory", "include");
        PublicSystemIncludePaths.Add(includeDirectory);
        if (!Target.bGenerateProjectFiles)
        {
            string libraryPath = Path.Combine(
                repositoryRoot,
                "Binaries",
                "NativeMemory",
                nativeToolchain,
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

            bool withTracy = Target.Configuration == UnrealTargetConfiguration.Debug ||
                             Target.Configuration == UnrealTargetConfiguration.DebugGame ||
                             Target.Configuration == UnrealTargetConfiguration.Development;
            if (withTracy)
            {
                string tracyDirectory = Path.Combine(
                    repositoryRoot,
                    "Binaries",
                    "Native",
                    "Tracy",
                    nativeToolchain,
                    Target.Platform.ToString(),
                    Target.Configuration.ToString());
                string tracyLibrary = Path.Combine(tracyDirectory, "SandboxTracyClient.lib");
                string tracyRuntime = Path.Combine(tracyDirectory, "SandboxTracyClient.dll");
                if (!File.Exists(tracyLibrary) || !File.Exists(tracyRuntime))
                {
                    throw new BuildException(
                        "NativeMemory expected the CMake-built Tracy library and runtime under '{0}'. " +
                        "Build Unreal targets through a repository CMake workflow.",
                        tracyDirectory);
                }

                PublicAdditionalLibraries.Add(tracyLibrary);
                PublicDelayLoadDLLs.Add("SandboxTracyClient.dll");
                RuntimeDependencies.Add("$(TargetOutputDir)/SandboxTracyClient.dll", tracyRuntime);
                ExternalDependencies.Add(tracyLibrary);
                ExternalDependencies.Add(tracyRuntime);
            }
        }
    }
}
