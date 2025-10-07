using UnrealBuildTool;

public class SandboxEditor : ModuleRules
{
    public SandboxEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        // Core dependencies
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "Sandbox", // Reference to runtime module
            "UnrealEd", // Must be public for class inheritance
            "EditorFramework", // Required by UnrealEd, must be public
            "Slate",
            "SlateCore",
            "BlueprintGraph", // Required by UnrealEd
            "MaterialEditor",
            "EditorSubsystem",
            "AssetRegistry", // For scanning data assets
            "ToolMenus" // For editor toolbar buttons
        });

        // Editor-specific dependencies
        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "MaterialEditor",
            "ToolWidgets",
            "EditorWidgets",
            "EditorStyle",
            "GraphEditor",
            "RenderCore", // Often needed for material nodes
            "EngineSettings", // Engine configuration access
            "USFLoaderEditor" // USF Loader plugin dependency
        });
    }
}