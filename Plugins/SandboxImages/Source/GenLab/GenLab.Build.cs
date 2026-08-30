using UnrealBuildTool;

public class GenLab : ModuleRules
{
    public GenLab(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Latest;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "AssetTools",
            "Engine",
            "ImageCore",
            "InputCore",
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
