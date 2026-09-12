// Copyright Epic Games, Inc. All Rights Reserved.

using Microsoft.Extensions.Logging;
using System;
using System.IO;
using System.Net.NetworkInformation;
using UnrealBuildTool;

public class SandboxNative : ModuleRules
{
    public SandboxNative(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Latest;

        bAllowUETypesInNamespaces = true;

        MinCpuArchX64 = MinimumCpuArchitectureX64.AVX2;


        PublicDependencyModuleNames.AddRange(new string[] {
            "Core",
        });

        PrivateDependencyModuleNames.AddRange(new string[] {
        });

        if (Target.Platform != UnrealTargetPlatform.Win64 ||
            Target.Architecture != UnrealArch.X64 ||
            Target.bUseStaticCRT ||
            (Target.Configuration == UnrealTargetConfiguration.Debug &&
             Target.bDebugBuildsActuallyUseDebugCRT))
        {
            throw new BuildException(
                "SandboxNative's native simulation library requires Win64 x64 with the dynamic release CRT.");
        }

        string repositoryRoot = Path.GetFullPath(Path.Combine(ModuleDirectory, "..", ".."));
        PublicSystemIncludePaths.Add(Path.Combine(repositoryRoot, "native", "simulation", "include"));

        if (!Target.bGenerateProjectFiles)
        {
            string nativeSimulationLibrary = Path.Combine(
                repositoryRoot,
                "Binaries",
                "Native",
                "Simulation",
                Target.Platform.ToString(),
                Target.Configuration.ToString(),
                "SandboxNativeSimulation.lib");
            if (!File.Exists(nativeSimulationLibrary))
            {
                throw new BuildException(
                    "SandboxNative expected the CMake-built native simulation library at '{0}'. " +
                    "Build Unreal targets through a repository CMake workflow.",
                    nativeSimulationLibrary);
            }
            PublicAdditionalLibraries.Add(nativeSimulationLibrary);
            ExternalDependencies.Add(nativeSimulationLibrary);
        }
    }
}
