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
            "UMG",
            "RenderCore",
            "TraceLog"            
        });

        PrivateDependencyModuleNames.AddRange(new string[] {
            "AIModule",
            "DeveloperSettings",
            "EnhancedInput",
            "GameplayTasks",
            "MassEntity",
            "MassCommon",
            "MassSimulation",
            "Niagara",           
            "NavigationSystem",            
            "Slate",
            "SlateCore"
        });

        // Uncomment if you are using online features
        // PrivateDependencyModuleNames.Add("OnlineSubsystem");

        // To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
    }
}
