using System;
using System.IO;
using UnrealBuildTool;

public class NativeSimulation : ModuleRules
{
    public NativeSimulation(ReadOnlyTargetRules Target) : base(Target)
    {
        Type = ModuleType.External;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "NativeMemory",
            "SandboxCore",
        });

        if (Target.Platform != UnrealTargetPlatform.Win64 ||
            Target.Architecture != UnrealArch.X64 ||
            Target.bUseStaticCRT ||
            (Target.Configuration == UnrealTargetConfiguration.Debug &&
             Target.bDebugBuildsActuallyUseDebugCRT))
        {
            throw new BuildException(
                "NativeSimulation requires Win64 x64 with the dynamic release CRT.");
        }

        string repositoryRoot = Path.GetFullPath(
            Path.Combine(ModuleDirectory, "..", "..", "..", ".."));
        string nativeToolchain =
            Environment.GetEnvironmentVariable("SANDBOX_NATIVE_TOOLCHAIN") ?? "clang-cl";
        PublicSystemIncludePaths.Add(
            Path.Combine(repositoryRoot, "native", "simulation", "include"));
        PublicSystemIncludePaths.Add(
            Path.Combine(repositoryRoot, "native", "lispb", "native_soa", "include"));
        PublicSystemIncludePaths.Add(
            Path.Combine(repositoryRoot, "native", "profiling", "include"));

        if (!Target.bGenerateProjectFiles)
        {
            string[] nativeLibraries = new string[]
            {
                Path.Combine(
                    repositoryRoot,
                    "Binaries",
                    "Native",
                    "Simulation",
                    nativeToolchain,
                    Target.Platform.ToString(),
                    Target.Configuration.ToString(),
                    "SandboxNativeSimulation.lib"),
                Path.Combine(
                    repositoryRoot,
                    "Binaries",
                    "Native",
                    "Core",
                    nativeToolchain,
                    Target.Platform.ToString(),
                    Target.Configuration.ToString(),
                    "SandboxNativeCore.lib"),
                Path.Combine(
                    repositoryRoot,
                    "Binaries",
                    "NativeMemory",
                    nativeToolchain,
                    Target.Platform.ToString(),
                    Target.Configuration.ToString(),
                    "SandboxNativeMemory.lib"),
            };

            foreach (string nativeLibrary in nativeLibraries)
            {
                if (!File.Exists(nativeLibrary))
                {
                    throw new BuildException(
                        "NativeSimulation expected a CMake-built native library at '{0}'. " +
                        "Build Unreal targets through a repository CMake workflow.",
                        nativeLibrary);
                }

                PublicAdditionalLibraries.Add(nativeLibrary);
                ExternalDependencies.Add(nativeLibrary);
            }

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
                        "NativeSimulation expected the CMake-built Tracy library and runtime under '{0}'. " +
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
