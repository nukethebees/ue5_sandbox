using UnrealBuildTool;

public class SandboxUITests : ModuleRules
{
    public SandboxUITests(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Latest;
        MinCpuArchX64 = MinimumCpuArchitectureX64.AVX2;

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
