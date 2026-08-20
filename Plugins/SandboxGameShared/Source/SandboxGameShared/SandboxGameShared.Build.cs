using UnrealBuildTool;

public class SandboxGameShared : ModuleRules
{
    public SandboxGameShared(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Latest;
        MinCpuArchX64 = MinimumCpuArchitectureX64.AVX2;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "SandboxCore",
            "SandboxCoreEngine",
            "Core",
            "CoreUObject",
            "DeveloperSettings",
            "Engine",
            "EnhancedInput",
            "Niagara",
            "Slate",
            "SlateCore",
            "UMG",
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "AssetRegistry",
        });
    }
}
