// Copyright Epic Games, Inc. All Rights Reserved.

using Microsoft.Extensions.Logging;
using System;
using System.Net.NetworkInformation;
using UnrealBuildTool;

public class SandboxTests : ModuleRules
{
    public SandboxTests(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Latest;

        bAllowUETypesInNamespaces = true;

        MinCpuArchX64 = MinimumCpuArchitectureX64.AVX2;

        CppCompileWarningSettings.AmbiguousReversedOperatorWarningLevel = WarningLevel.Error;
        CppCompileWarningSettings.UnreachableCodeWarningLevel = WarningLevel.Error;
        CppCompileWarningSettings.ShortenSizeTToIntWarningLevel = WarningLevel.Error;

        PublicDependencyModuleNames.AddRange(new string[] {
        });

        PrivateDependencyModuleNames.AddRange(new string[] {
            "SandboxCore",
            "Sandbox",
            "Core",
            "CoreUObject",
            "Engine",
            "RenderCore",
            "FunctionalTesting",
            "UnrealEd",
            "CQTest",
        });
    }
}
