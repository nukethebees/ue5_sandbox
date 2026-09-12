// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class USFLoader : ModuleRules
{
    public USFLoader(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Latest;

        PublicDependencyModuleNames.Add("Core");

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Projects",
            "RenderCore",
        });
    }
}
