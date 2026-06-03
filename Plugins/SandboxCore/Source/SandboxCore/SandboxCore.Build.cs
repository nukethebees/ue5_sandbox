// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SandboxCore : ModuleRules
{
    public SandboxCore(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
        MinCpuArchX64 = MinimumCpuArchitectureX64.AVX2;
        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "Engine",
            }
            );
    }
}
