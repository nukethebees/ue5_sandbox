using UnrealBuildTool;

public class LuaLabEditor : ModuleRules
{
    public LuaLabEditor(ReadOnlyTargetRules Target) : base(Target)
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
            "LuaLab",
            "Projects",
            "Slate",
            "SlateCore",
            "UnrealEd",
        });
    }
}
