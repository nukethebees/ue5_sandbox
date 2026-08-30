using UnrealBuildTool;

public class SandboxNiagaraEditor : ModuleRules
{
    public SandboxNiagaraEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Latest;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Blutility",
            "EditorSubsystem",
            "SlateCore",
            "UnrealEd",
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "AdvancedPreviewScene",
            "Engine",
            "AssetRegistry",
            "BlueprintEditorLibrary",
            "InputCore",
            "Json",
            "Niagara",
            "NiagaraEditor",
            "PropertyEditor",
            "Projects",
            "Slate",
            "UMG",
            "UMGEditor",
        });
    }
}
