// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

[SupportedPlatforms(UnrealPlatformClass.All)]
public class SandboxCoreTestsTarget : TestTargetRules
{
    public SandboxCoreTestsTarget(TargetInfo Target) : base(Target)
    {
        bNeverCompileAgainstEngine = true;
        bNeverCompileAgainstCoreUObject = true;

        bTestsRequireEditor = false;
        bTestsRequireEngine = false;
        bTestsRequireCoreUObject = false;

        bCompileAgainstEngine = false;
        bCompileAgainstCoreUObject = false;

        bMockEngineDefaults = true;
        bUsePlatformFileStub = true;

        GlobalDefinitions.Add("CATCH_CONFIG_ENABLE_BENCHMARKING=1");
    }
}
