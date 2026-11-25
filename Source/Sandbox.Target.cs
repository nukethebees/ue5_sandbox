// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

[SupportedPlatforms("Win64")]
public class SandboxTarget : TargetRules
{
    public SandboxTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        CppStandard = CppStandardVersion.Latest;
        BuildEnvironment = TargetBuildEnvironment.Unique;
        LinkType = TargetLinkType.Monolithic;
        bShouldCompileAsDLL = true;
        GlobalDefinitions.Add("UE_LIBRARY_ENABLED=1");
        ExtraModuleNames.AddRange(new[] { "Sandbox", "UELibrary" });
    }
}
