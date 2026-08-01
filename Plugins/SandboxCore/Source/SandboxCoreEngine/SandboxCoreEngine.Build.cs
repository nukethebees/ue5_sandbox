using UnrealBuildTool;

public class SandboxCoreEngine : ModuleRules
{
    public SandboxCoreEngine(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        MinCpuArchX64 = MinimumCpuArchitectureX64.AVX2;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "SandboxCore",
            "Core",
            "CoreUObject",
            "Engine",
        });
    }
}
