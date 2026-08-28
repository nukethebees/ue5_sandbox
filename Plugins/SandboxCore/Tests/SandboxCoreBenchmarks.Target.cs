// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

[SupportedPlatforms(UnrealPlatformClass.All)]
public class SandboxCoreBenchmarksTarget : TestTargetRules
{
    public SandboxCoreBenchmarksTarget(TargetInfo Target) : base(Target)
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
