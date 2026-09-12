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
        string packageRoot = Path.Combine(repositoryRoot, "vcpkg_installed", "x64-windows");
        bool debugCrt = Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT;
        string variantRoot = debugCrt ? Path.Combine(packageRoot, "debug") : packageRoot;
        string dllName = debugCrt ? "mimalloc-debug.dll" : "mimalloc.dll";
        string dll = Path.Combine(variantRoot, "bin", dllName);
        if (!File.Exists(dll))
        {
            throw new BuildException("Configure an Unreal CMake preset to provision mimalloc: missing {0}", dll);
        }
        PrivateIncludePaths.Add(Path.Combine(packageRoot, "include"));
        RuntimeDependencies.Add("$(TargetOutputDir)/sbx-mimalloc.dll", dll);
        RuntimeDependencies.Add("$(TargetOutputDir)/mimalloc-redirect.dll", Path.Combine(variantRoot, "bin", "mimalloc-redirect.dll"));
    }
}
