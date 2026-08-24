using UnrealBuildTool;

public class SandboxUITests : ModuleRules
{
    public SandboxUITests(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Latest;

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "CQTest",
            "Engine",
            "SandboxCore",
            "SandboxUI",
            "SlateCore",
        });
    }
}
