// Copyright Epic Games, Inc. All Rights Reserved.

using Microsoft.Extensions.Logging;
using System;
using System.Net.NetworkInformation;
using UnrealBuildTool;

public class Sandbox : ModuleRules
{
    public Sandbox(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Latest;

        MinCpuArchX64 = MinimumCpuArchitectureX64.AVX2;

        CppCompileWarningSettings.AmbiguousReversedOperatorWarningLevel = WarningLevel.Error;
        CppCompileWarningSettings.UnreachableCodeWarningLevel = WarningLevel.Error;
        CppCompileWarningSettings.ShortenSizeTToIntWarningLevel = WarningLevel.Error;

        PublicDependencyModuleNames.AddRange(new string[] {
            "SandboxCore",
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

        Target.Logger.LogInformation("=== Sandbox Build.cs [Start] ===");
        Target.Logger.LogInformation($"OptimizeCode : {OptimizeCode}");
        Target.Logger.LogInformation($"OptimizationLevel : {OptimizationLevel}");
        Target.Logger.LogInformation($"PCHUsage : {PCHUsage}");
        Target.Logger.LogInformation($"bTreatAsEngineModule : {bTreatAsEngineModule}");
        Target.Logger.LogInformation($"bUseRTTI : {bUseRTTI}");
        Target.Logger.LogInformation($"MinCpuArchX64 : {MinCpuArchX64}");
        Target.Logger.LogInformation($"bEnableExceptions  : {bEnableExceptions}");
        Target.Logger.LogInformation($"CppCompileWarningSettings : {CppCompileWarningSettings}");
        Target.Logger.LogInformation($"bWarningsAsErrors : {bWarningsAsErrors}");
        Target.Logger.LogInformation($"bDisableStaticAnalysis : {bDisableStaticAnalysis}");
        Target.Logger.LogInformation($"bStaticAnalyzerExtensions : {bStaticAnalyzerExtensions}");
        Target.Logger.LogInformation($"bUseUnity : {bUseUnity}");
        Target.Logger.LogInformation($"IWYUSupport : {IWYUSupport}");
        Target.Logger.LogInformation($"bPrecompile : {bPrecompile}");
        Target.Logger.LogInformation($"bUsePrecompiled : {bUsePrecompiled}");
        Target.Logger.LogInformation($"bAlwaysExportStructs : {bAlwaysExportStructs}");
        Target.Logger.LogInformation($"bAlwaysExportEnums : {bAlwaysExportEnums}");
        Target.Logger.LogInformation($"bAllowUETypesInNamespaces : {bAllowUETypesInNamespaces}");
        Target.Logger.LogInformation($"PrivateDefinitions : {PrivateDefinitions}");
        Target.Logger.LogInformation($"PublicDefinitions : {PublicDefinitions}");
        Target.Logger.LogInformation($"CppStandard : {CppStandard}");
        Target.Logger.LogInformation($"CStandard : {CStandard}");
        Target.Logger.LogInformation($"EngineDirectory : {EngineDirectory}");

        Target.Logger.LogInformation($"Target.Platform : {Target.Platform}");
        Target.Logger.LogInformation($"Target.bBuildEditor : {Target.bBuildEditor}");
        //Console.WriteLine($" : {}");
        Target.Logger.LogInformation("=== Sandbox Build.cs [End] ===");
    }
}
