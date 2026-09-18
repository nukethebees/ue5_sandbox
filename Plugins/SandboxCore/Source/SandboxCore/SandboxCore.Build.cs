// Copyright Epic Games, Inc. All Rights Reserved.

using Microsoft.Extensions.Logging;
using System;
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
        string nativeToolchain = Environment.GetEnvironmentVariable("SANDBOX_NATIVE_TOOLCHAIN") ?? "clang-cl";
        PublicSystemIncludePaths.Add(Path.Combine(repositoryRoot, "native", "core", "include"));
        PublicSystemIncludePaths.Add(Path.Combine(repositoryRoot, "native", "simulation", "include"));
        PublicSystemIncludePaths.Add(
            Path.Combine(repositoryRoot, "native", "lispb", "native_soa", "include"));
        PublicSystemIncludePaths.Add(
            Path.Combine(repositoryRoot, "native", "profiling", "include"));
        PublicSystemIncludePaths.Add(
            Path.Combine(repositoryRoot, "native", "third_party", "handmade_math"));

        if (!Target.bGenerateProjectFiles)
        {
            string nativeCoreLibrary = Path.Combine(
                repositoryRoot,
                "Binaries",
                "Native",
                "Core",
                nativeToolchain,
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
                nativeToolchain,
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

        bool withTracy = Target.Configuration == UnrealTargetConfiguration.Debug ||
                         Target.Configuration == UnrealTargetConfiguration.DebugGame ||
                         Target.Configuration == UnrealTargetConfiguration.Development;
        if (withTracy)
        {
            PrivateIncludePaths.Add(
                Path.Combine(repositoryRoot, "native", "third_party", "tracy", "public"));
            PrivateDefinitions.AddRange(new string[]
            {
                "SANDBOX_WITH_TRACY",
                "TRACY_ENABLE",
                "TRACY_IMPORTS",
                "TRACY_MANUAL_LIFETIME",
                "TRACY_NO_BROADCAST",
                "TRACY_NO_CRASH_HANDLER",
                "TRACY_ON_DEMAND",
                "TRACY_ONLY_LOCALHOST",
            });
        }
        if (withTracy && !Target.bGenerateProjectFiles)
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
                    "SandboxCore expected the CMake-built Tracy library and runtime under '{0}'. " +
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
