// Copyright Epic Games, Inc. All Rights Reserved.

using Microsoft.Extensions.Logging;
using System.IO;
using UnrealBuildTool;

public class SandboxCore : ModuleRules
{
    public SandboxCore(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Latest;
        MinCpuArchX64 = MinimumCpuArchitectureX64.AVX2;
        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
            }
            );

        if (Target.Platform != UnrealTargetPlatform.Win64 ||
            Target.Architecture != UnrealArch.X64 ||
            Target.bUseStaticCRT ||
            (Target.Configuration == UnrealTargetConfiguration.Debug &&
             Target.bDebugBuildsActuallyUseDebugCRT))
        {
            throw new BuildException(
                "SandboxCore's native library requires Win64 x64 with the dynamic release CRT.");
        }

        string repositoryRoot = Path.GetFullPath(Path.Combine(ModuleDirectory, "..", "..", "..", ".."));
        PublicSystemIncludePaths.Add(Path.Combine(repositoryRoot, "native", "core", "include"));
        PublicSystemIncludePaths.Add(
            Path.Combine(repositoryRoot, "native", "third_party", "handmade_math"));

        if (!Target.bGenerateProjectFiles)
        {
            string nativeCoreLibrary = Path.Combine(
                repositoryRoot,
                "Binaries",
                "Native",
                "Core",
                Target.Platform.ToString(),
                Target.Configuration.ToString(),
                "SandboxNativeCore.lib");
            if (!File.Exists(nativeCoreLibrary))
            {
                throw new BuildException(
                    "SandboxCore expected the CMake-built native core library at '{0}'. " +
                    "Build Unreal targets through a repository CMake workflow.",
                    nativeCoreLibrary);
            }
            PublicAdditionalLibraries.Add(nativeCoreLibrary);
            ExternalDependencies.Add(nativeCoreLibrary);
        }

        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            Target.Logger.LogInformation($"Target.WindowsPlatform.ToolChainDir : {Target.WindowsPlatform.ToolChainDir}");
            Target.Logger.LogInformation($"Target.WindowsPlatform.WindowsSdkDir : {Target.WindowsPlatform.WindowsSdkDir}");
            Target.Logger.LogInformation($"Target.WindowsPlatform.WindowsSdkVersion : {Target.WindowsPlatform.WindowsSdkVersion}");

            string OneCoreLib = Path.Combine(Target.WindowsPlatform.WindowsSdkDir,
                "Lib",
                Target.WindowsPlatform.WindowsSdkVersion,
                "um",
                "x64",
                "OneCore.lib"
            );

            PublicAdditionalLibraries.Add(OneCoreLib);
        }
        string includeDirectory = Path.Combine(repositoryRoot, "native", "sbx_mimalloc", "include");
        PrivateIncludePaths.Add(includeDirectory);
        if (!Target.bGenerateProjectFiles)
        {
            string libraryPath = Path.Combine(
                repositoryRoot,
                "Binaries",
                "Native",
                "SbxMimalloc",
                Target.Platform.ToString(),
                Target.Configuration.ToString(),
                "SandboxMimalloc.lib");
            if (!File.Exists(libraryPath))
            {
                throw new BuildException(
                    "SandboxCore expected the private mimalloc library at '{0}'. " +
                    "Build Unreal targets through a repository CMake workflow.",
                    libraryPath);
            }
            PublicAdditionalLibraries.Add(libraryPath);
            ExternalDependencies.Add(libraryPath);
        }
        PublicSystemLibraries.AddRange(
            new string[]
            {
                "psapi.lib",
                "shell32.lib",
                "user32.lib",
                "advapi32.lib",
                "bcrypt.lib",
            }
            );
    }
}
