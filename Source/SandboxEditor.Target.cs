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
        ExtraModuleNames.AddRange(new string[] { "Sandbox", "SandboxEditor" });

        if (BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Win64)
        {
            PreBuildSteps.Add("cd /d \"$(ProjectDir)\" && py -3 -m Codegen.generate");
        }
        else
        {
            PreBuildSteps.Add("cd \"$(ProjectDir)\" && python3 -m Codegen.generate");
        }
    }
}
