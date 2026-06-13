// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SandboxCoreTests : TestModuleRules
{
	static SandboxCoreTests()
	{
		if (InTestMode)
		{
			TestMetadata = new Metadata();
			TestMetadata.TestName = "SandboxCore";
			TestMetadata.TestShortName = "Sandbox Core";
		}
	}

	public SandboxCoreTests(ReadOnlyTargetRules Target) : base(Target, true)
	{
		bAllowUETypesInNamespaces = true;

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"SandboxCore",
			"Engine",
			"Core",
			"SlateCore",
		});
    }
}
