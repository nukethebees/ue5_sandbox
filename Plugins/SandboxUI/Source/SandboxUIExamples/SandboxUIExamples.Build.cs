using UnrealBuildTool;

public class SandboxUIExamples : ModuleRules
{
    public SandboxUIExamples(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Latest;
        MinCpuArchX64 = MinimumCpuArchitectureX64.AVX2;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "UMG",
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "SandboxCore",
            "SandboxUI",
            "Slate",
            "SlateCore",
        });
    }
}
