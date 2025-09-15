using UnrealBuildTool;

public class USFLoaderEditor : ModuleRules
{
    public USFLoaderEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "UnrealEd",
                "MaterialEditor",
                "KismetCompiler",
                "GraphEditor",
                "Slate",
                "SlateCore",
                "EditorStyle",
                "EditorWidgets",
                "ToolMenus",
                "USFLoader"
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "EditorSubsystem",
                "RenderCore",
                "RHI",
                "Projects",
                "ShaderCompilerCommon"
            }
        );
    }
}