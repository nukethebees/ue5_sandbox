// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SandboxNativeTests : TestModuleRules
{
    static SandboxNativeTests()
    {
        if (InTestMode)
        {
            TestMetadata = new Metadata();
            TestMetadata.TestName = "SandboxNativeTests";
            TestMetadata.TestShortName = "SandboxNativeLLTs";
        }
    }

    public SandboxNativeTests(ReadOnlyTargetRules Target) : base(Target, true)
    {
        CppStandard = CppStandardVersion.Latest;
        bAllowUETypesInNamespaces = true;

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "SandboxNative",
        });
    }
}
