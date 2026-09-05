using UnrealBuildTool;

public class SGLegacy : ModuleRules
{
    public SGLegacy(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Latest;
        MinCpuArchX64 = MinimumCpuArchitectureX64.AVX2;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "Sandbox",
            "SandboxCoreEngine",
            "SandboxGameShared",
            "SpaceGame",
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "EnhancedInput",
            "InputCore",
            "Niagara",
            "RenderCore",
            "SandboxCore",
            "SandboxNative",
            "Slate",
            "SlateCore",
            "UMG",
        });

        if (Target.bBuildEditor)
        {
            PrivateDependencyModuleNames.Add("UnrealEd");
        }
    }
}
