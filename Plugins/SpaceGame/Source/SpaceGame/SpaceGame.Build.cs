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
            "Core",
            "CoreUObject",
            "EnhancedInput",
            "Engine",
            "InputCore",
            "RenderCore",
            "UMG",
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
