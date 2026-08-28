using UnrealBuildTool;

public class GenLab : ModuleRules
{
    public GenLab(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Latest;

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "AssetTools",
            "Core",
            "CoreUObject",
            "CQTest",
            "Engine",
            "ImageCore",
            "LevelEditor",
            "Projects",
            "PropertyEditor",
            "SandboxImages",
            "Slate",
            "SlateCore",
            "ToolMenus",
            "UnrealEd",
            "WorkspaceMenuStructure",
        });
    }
}
