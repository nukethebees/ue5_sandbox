// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SandboxCoreBenchmarks : TestModuleRules
{
    static SandboxCoreBenchmarks()
    {
        if (InTestMode)
        {
            TestMetadata = new Metadata();
            TestMetadata.TestName = "SandboxCoreBenchmarks";
            TestMetadata.TestShortName = "Sandbox Core Benchmarks";
        }
    }

    public SandboxCoreBenchmarks(ReadOnlyTargetRules Target) : base(Target, true)
    {
        CppStandard = CppStandardVersion.Latest;
        bAllowUETypesInNamespaces = true;

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "SandboxCore",
        });
    }
}
