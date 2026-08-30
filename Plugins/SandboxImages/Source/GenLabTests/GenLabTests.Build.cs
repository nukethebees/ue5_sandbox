using UnrealBuildTool;

public class GenLabTests : ModuleRules
{
    public GenLabTests(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Latest;

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "CQTest",
            "Engine",
            "GenLab",
        });
    }
}
