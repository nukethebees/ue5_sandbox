// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Sandbox : ModuleRules
    {
    public Sandbox(ReadOnlyTargetRules Target) : base(Target)
        {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[] {
            "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "UMG", "RenderCore"
        });

        PrivateDependencyModuleNames.AddRange(new string[] {
            "UnrealEd",           // For UMaterialGraphNode_Base
            "MaterialEditor",     // For SGraphNodeMaterialBase
            "ToolWidgets",        // For FHLSLSyntaxHighlighterMarshaller
            "EditorWidgets",      // For FEdGraphUtilities
            "Slate", "SlateCore",
            "EditorFramework",
            "GraphEditor"
        });


        CppStandard = CppStandardVersion.Latest;

        // Uncomment if you are using online features
        // PrivateDependencyModuleNames.Add("OnlineSubsystem");

        // To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
        }
    }
