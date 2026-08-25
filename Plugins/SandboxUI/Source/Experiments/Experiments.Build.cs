using UnrealBuildTool;

public class Experiments : ModuleRules
{
    public Experiments(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Latest;
        MinCpuArchX64 = MinimumCpuArchitectureX64.AVX2;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "SlateCore",
            "UMG",
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Blutility",
            "Engine",
            "Projects",
            "RenderCore",
            "RHI",
            "Slate",
            "UnrealEd",
            "WorkspaceMenuStructure",
        });
    }
}
