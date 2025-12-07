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
            "InputCore", 
            "EnhancedInput", 
            "UMG", 
            "RenderCore",
            "Slate", 
            "SlateCore", 
            "Niagara", 
            "AIModule", 
            "NavigationSystem", 
            "GameplayTasks",
            "TraceLog", 
            "MassEntity", 
            "MassCommon", 
            "MassSimulation"
        });

        PrivateDependencyModuleNames.AddRange(new string[] {
            // Additional private dependencies
        });

        // Uncomment if you are using online features
        // PrivateDependencyModuleNames.Add("OnlineSubsystem");

        // To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
    }
}
