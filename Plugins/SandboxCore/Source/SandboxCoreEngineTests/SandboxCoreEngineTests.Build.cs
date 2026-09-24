// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SandboxCoreEngineTests : ModuleRules
{
    public SandboxCoreEngineTests(ReadOnlyTargetRules Target) : base(Target)
    {
        CppStandard = CppStandardVersion.Latest;
        bAllowUETypesInNamespaces = true;
        MinCpuArchX64 = MinimumCpuArchitectureX64.AVX2;

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "CQTest",
            "Engine",
            "UnrealEd",
            "SandboxCore",
            "SandboxCoreEngine",
        });
    }
}
