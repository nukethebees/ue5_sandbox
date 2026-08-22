// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

[SupportedPlatforms(UnrealPlatformClass.All)]
public class SandboxNativeTestsTarget : TestTargetRules
{
    public SandboxNativeTestsTarget(TargetInfo Target) : base(Target)
    {
        bTestsRequireEditor = false;
        bTestsRequireEngine = false;
    }
}
