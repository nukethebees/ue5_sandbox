using UnrealBuildTool;

public class SandboxShadersEditor : ModuleRules
{
    public SandboxShadersEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivatePCHHeaderFile = "Private/SandboxShadersEditorPCH.h";
        CppStandard = CppStandardVersion.Latest;
        MinCpuArchX64 = MinimumCpuArchitectureX64.AVX2;

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "CQTest",
            "Engine",
            "LevelEditor",
            "MaterialEditor",
            "Projects",
            "RenderCore",
            "RHI",
            "SandboxShaders",
            "SbxShadersExperiments",
            "Slate",
            "SlateCore",
            "ToolMenus",
            "UnrealEd",
        });
    }
}
