// Copyright Epic Games, Inc. All Rights Reserved.

using Microsoft.Extensions.Logging;
using System;
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
    }
}
