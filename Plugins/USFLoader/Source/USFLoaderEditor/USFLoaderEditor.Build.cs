using UnrealBuildTool;

public class USFLoaderEditor : ModuleRules
{
    public USFLoaderEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Latest;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "MaterialEditor",
            "Slate",
            "SlateCore",
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "CQTest",
            "GraphEditor",
            "RenderCore",
            "UnrealEd",
            "USFLoader",
        });
    }
}
