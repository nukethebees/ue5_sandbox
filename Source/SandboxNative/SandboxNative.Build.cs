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
            "SandboxCore",
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
        string nativeToolchain = Environment.GetEnvironmentVariable("SANDBOX_NATIVE_TOOLCHAIN") ?? "clang-cl";
        PublicSystemIncludePaths.Add(Path.Combine(repositoryRoot, "native", "simulation", "include"));
        PublicSystemIncludePaths.Add(
            Path.Combine(repositoryRoot, "native", "lispb", "native_soa", "include"));

        if (!Target.bGenerateProjectFiles)
        {
            string[] nativeLibraries = new string[] {
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
                        "SandboxNative expected a CMake-built native library at '{0}'. " +
                        "Build Unreal targets through a repository CMake workflow.",
                        nativeLibrary);
                }

                PublicAdditionalLibraries.Add(nativeLibrary);
                ExternalDependencies.Add(nativeLibrary);
            }
        }
    }
}
