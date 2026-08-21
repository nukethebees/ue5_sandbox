// Copyright Epic Games, Inc. All Rights Reserved.

using Microsoft.Extensions.Logging;
using System;
using System.Net.NetworkInformation;
using UnrealBuildTool;

public class ShooterGame: ModuleRules
{
    public ShooterGame(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Latest;

        PublicDependencyModuleNames.AddRange(new string[] {
            "SandboxGameShared",
            "SandboxNative",
            "SandboxCore",
            "SandboxCoreEngine",
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
            "UMG",
        });

        if (Target.bBuildEditor)
        {
            // Scoped transaction
            PrivateDependencyModuleNames.AddRange(new string[] { 
                "UnrealEd", 
                "FunctionalTesting", 
            });
        }
        
    }
}
