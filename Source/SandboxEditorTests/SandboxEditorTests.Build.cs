using UnrealBuildTool;

public class SandboxEditorTests : ModuleRules
{
    public SandboxEditorTests(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivatePCHHeaderFile = "Private/SandboxEditorTestsPCH.h";
        CppStandard = CppStandardVersion.Latest;
        MinCpuArchX64 = MinimumCpuArchitectureX64.AVX2;

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "SandboxEditor",
            "SandboxShaders",
            "SandboxCore",
            "NativeSimulation",
            "NativeLevelAuthoring",
            "NativePresentation",
            "SpaceGame",
            "SpaceGamePresentation",
            "SpaceGameRendering",
            "SpaceGameSimulation",
            "Core",
            "CoreUObject",
            "Engine",
            "UMG",
            "UMGEditor",
            "UnrealEd",
            "AssetRegistry",
            "CQTest",
            "MaterialEditor",
            "PropertyEditor",
            "RenderCore",
            "RHI",
            "Slate",
            "SlateCore",
        });
    }
}
