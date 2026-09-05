using UnrealBuildTool;
using System.IO;

public class SpaceGame : ModuleRules
{
    public SpaceGame(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Latest;
        bAllowUETypesInNamespaces = true;
        MinCpuArchX64 = MinimumCpuArchitectureX64.AVX2;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "SandboxCore",
            "SandboxCoreEngine",
            "SandboxGameShared",
            "SandboxISMC",
            "SandboxNative",
            "SandboxUI",
            "CommonInput",
            "CommonUI",
            "Core",
            "CoreUObject",
            "DeveloperSettings",
            "EnhancedInput",
            "Engine",
            "GameplayTags",
            "InputCore",
            "RenderCore",
            "SandboxUI",
            "SlateCore",
            "UMG",
            "SGCollision",
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Niagara",
            "Slate",
            "TraceLog",
            "Projects",
        });

        string hiveIconDirectory = Path.Combine(PluginDirectory, "Resources", "UI", "Hive");
        foreach (string hiveIcon in Directory.GetFiles(hiveIconDirectory, "*.svg"))
        {
            RuntimeDependencies.Add(hiveIcon);
        }

        if (Target.bBuildEditor)
        {
            PrivateDependencyModuleNames.Add("UnrealEd");
        }
    }
}
