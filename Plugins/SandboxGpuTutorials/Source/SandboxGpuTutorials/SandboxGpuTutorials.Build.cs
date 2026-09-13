using UnrealBuildTool;

public class SandboxGpuTutorials : ModuleRules
{
    public SandboxGpuTutorials(ReadOnlyTargetRules Target) : base(Target)
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
            "LevelEditor",
            "Projects",
            "RenderCore",
            "SandboxUI",
            "Slate",
            "SlateCore",
            "ToolMenus",
            "WorkspaceMenuStructure",
        });
    }
}
