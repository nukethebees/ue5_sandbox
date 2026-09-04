using UnrealBuildTool;

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
            "SandboxNative",
            "CommonInput",
            "CommonUI",
            "Core",
            "CoreUObject",
            "EnhancedInput",
            "Engine",
            "GameplayTags",
            "InputCore",
            "RenderCore",
            "UMG",
            "SGCollision",
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "DeveloperSettings",
            "Niagara",
            "SandboxUI",
            "Slate",
            "SlateCore",
            "TraceLog",
        });

        if (Target.bBuildEditor)
        {
            PrivateDependencyModuleNames.Add("UnrealEd");
        }
    }
}
