using UnrealBuildTool;

public class SandboxGpuTutorials : ModuleRules
{
    public SandboxGpuTutorials(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Latest;

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
