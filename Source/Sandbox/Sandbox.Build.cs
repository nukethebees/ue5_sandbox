// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Sandbox : ModuleRules
{
    public Sandbox(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Latest;

        PublicDependencyModuleNames.AddRange(new string[] {
            "Core", 
            "CoreUObject", 
            "Engine", 
            "RenderCore"
        });

        PrivateDependencyModuleNames.AddRange(new string[] {
            "AIModule",
            "DeveloperSettings",
            "EnhancedInput",
            "GameplayTasks",
            "InputCore",
            "MassEntity",
            "MassCore",
            "MassCommon",
            "MassSimulation",
            "Niagara",           
            "NavigationSystem",            
            "Slate",
            "SlateCore",
            "TraceLog",
            "UMG"
        });

        
    }
}
