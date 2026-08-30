using UnrealBuildTool;

public class S7LabEditor : ModuleRules
{
    public S7LabEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Latest;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Blutility",
            "UMG",
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "AssetRegistry",
            "Engine",
            "InputCore",
            "Projects",
            "S7Lab",
            "Slate",
            "SlateCore",
            "UnrealEd",
        });
    }
}
