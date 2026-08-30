using UnrealBuildTool;

public class SbxMeshGenLab : ModuleRules
{
    public SbxMeshGenLab(ReadOnlyTargetRules Target) : base(Target)
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
            "AssetRegistry",
            "Blutility",
            "CQTest",
            "EditorFramework",
            "Engine",
            "InputCore",
            "MeshDescription",
            "PropertyEditor",
            "Projects",
            "Slate",
            "SlateCore",
            "StaticMeshDescription",
            "ToolMenus",
            "UMG",
            "UnrealEd",
            "WorkspaceMenuStructure",
        });
    }
}
