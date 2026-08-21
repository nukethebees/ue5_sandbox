using UnrealBuildTool;

public class ShooterGameEditor : ModuleRules
{
    public ShooterGameEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Latest;
        MinCpuArchX64 = MinimumCpuArchitectureX64.AVX2;

        // Core dependencies
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "ShooterGame",
            "Core",
            "CoreUObject",
            "Engine",
        });

        // Editor-specific dependencies
        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "AssetRegistry",
            "LevelEditor",
            "PropertyEditor",
            "UnrealEd",
            "Slate",
            "SlateCore",
            "SandboxGameShared",
        });
    }
}
