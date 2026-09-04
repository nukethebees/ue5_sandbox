using UnrealBuildTool;

public class SbxUIExperiments : ModuleRules
{
    public SbxUIExperiments(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Latest;
        MinCpuArchX64 = MinimumCpuArchitectureX64.AVX2;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Blutility",
            "SlateCore",
            "UMG",
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "AssetRegistry",
            "Engine",
            "Projects",
            "RenderCore",
            "RHI",
            "SandboxUI",
            "Slate",
            "UMGEditor",
            "UnrealEd",
        });
    }
}
