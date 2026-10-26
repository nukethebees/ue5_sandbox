using UnrealBuildTool;

public class SandboxISMCLab : ModuleRules
{
    public SandboxISMCLab(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivatePCHHeaderFile = "Private/SandboxISMCLabPCH.h";
        CppStandard = CppStandardVersion.Latest;
        MinCpuArchX64 = MinimumCpuArchitectureX64.AVX2;

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "RenderCore",
            "RHI",
            "Json",
            "SandboxISMC",
            "UnrealEd",
        });
    }
}
