// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class SandboxEditorTarget : TargetRules
{
    public SandboxEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        CppStandard = CppStandardVersion.Latest;
        bUseUnityBuild = true;
        NumIncludedBytesPerUnityCPP = 65536;
        MinGameModuleSourceFilesForUnityBuild = 8;
        ExtraModuleNames.AddRange(new string[] { "Sandbox", "SandboxEditor" });

    }
}
