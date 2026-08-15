// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class SandboxTarget : TargetRules
{
    public SandboxTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.Add("Sandbox");

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
