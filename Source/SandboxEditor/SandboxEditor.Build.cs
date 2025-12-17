using UnrealBuildTool;

public class SandboxEditor : ModuleRules
{
    public SandboxEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Latest;

        // Core dependencies
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "UnrealEd", // Must be public for class inheritance
            "EditorFramework" // Required by UnrealEd, must be public
        });

        // Editor-specific dependencies
        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "AssetRegistry", // For scanning data assets
            "BlueprintGraph", // Required by UnrealEd
            "EditorWidgets",
            "EditorSubsystem",
            "EditorStyle",
            "EngineSettings", // Engine configuration access
            "GraphEditor",
            "MaterialEditor",
            "PropertyEditor",
            "RenderCore", // Often needed for material nodes
            "Sandbox", // Reference to runtime module
            "Slate",
            "SlateCore",
            "ToolWidgets",
            "ToolMenus", // For editor toolbar buttons
            "USFLoaderEditor" // USF Loader plugin dependency            
        });
    }
}