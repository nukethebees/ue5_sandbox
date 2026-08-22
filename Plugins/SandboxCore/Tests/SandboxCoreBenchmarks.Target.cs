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
