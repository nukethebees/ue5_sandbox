using UnrealBuildTool;
using System.IO;

public class SpaceGame : ModuleRules
{
    public SpaceGame(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivatePCHHeaderFile = "Private/SpaceGamePCH.h";
        CppStandard = CppStandardVersion.Latest;
        bAllowUETypesInNamespaces = true;
        MinCpuArchX64 = MinimumCpuArchitectureX64.AVX2;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "SpaceGameSimulation",
            "SpaceGamePresentation",
            "SandboxCore",
            "SandboxCoreEngine",
            "SandboxGameShared",
            "SandboxISMC",
            "SandboxShaders",
            "NativeSimulation",
            "NativeLevelAuthoring",
            "NativeProfiles",
            "NativePresentation",
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
            "SlateCore",
            "SpaceGameRendering",
            "UMG",
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "ApplicationCore",
            "CpuFeatures",
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

        if (Target.Type == TargetType.Game)
        {
            PrivateDefinitions.Add("SANDBOX_HOST_EXPORTS");
            PrivateIncludePaths.Add(Path.Combine(Target.ProjectFile.Directory.FullName, "native", "sandbox_host", "include"));
            PrivateIncludePaths.Add(Path.Combine(Target.ProjectFile.Directory.FullName, "Intermediate", "SandboxHost"));
        }

        if (Target.bBuildEditor)
        {
            PrivateDependencyModuleNames.Add("UnrealEd");
        }
    }
}
