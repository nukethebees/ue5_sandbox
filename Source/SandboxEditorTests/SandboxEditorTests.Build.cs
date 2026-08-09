using UnrealBuildTool;

public class SandboxEditorTests : ModuleRules
{
    public SandboxEditorTests(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Latest;
        MinCpuArchX64 = MinimumCpuArchitectureX64.AVX2;

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "SandboxEditor",
            "Core",
            "CoreUObject",
            "Engine",
            "UMG",
            "UMGEditor",
            "UnrealEd",
            "AssetRegistry",
            "CQTest",
        });
    }
}
