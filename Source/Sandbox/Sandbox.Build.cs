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
            "GameplayStateTreeModule",
            "GameplayTasks",
            "InputCore",
            "MassEntity",
            "MassCommon",
            "MassSimulation",
            "Niagara",           
            "NavigationSystem",            
            "Slate",
            "SlateCore",
            "StateTreeModule",
            "TraceLog",
            "UMG"
        });

        // Uncomment if you are using online features
        // PrivateDependencyModuleNames.Add("OnlineSubsystem");

        // To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
    }
}
