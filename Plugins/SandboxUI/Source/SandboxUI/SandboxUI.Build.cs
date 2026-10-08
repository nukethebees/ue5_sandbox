using UnrealBuildTool;

public class SandboxUI : ModuleRules
{
    public SandboxUI(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Latest;
        MinCpuArchX64 = MinimumCpuArchitectureX64.AVX2;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "InputCore",
            "SandboxCore",
            "NativeUI",
            "Slate",
            "SlateCore",
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Engine",
            "Projects",
            "RenderCore",
            "RHI",
        });
    }
}
