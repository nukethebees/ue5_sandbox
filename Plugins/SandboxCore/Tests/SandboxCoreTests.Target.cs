// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

[SupportedPlatforms(UnrealPlatformClass.All)]
public class SandboxCoreTestsTarget : TestTargetRules
{
    public SandboxCoreTestsTarget(TargetInfo Target) : base(Target)
    {
        bNeverCompileAgainstEngine = false;
        bNeverCompileAgainstCoreUObject = true;

        GlobalDefinitions.Add("CATCH_CONFIG_ENABLE_BENCHMARKING=1");
    }
}
