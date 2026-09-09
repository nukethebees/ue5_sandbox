using UnrealBuildTool;

public class SpaceGamePresentation : ModuleRules
{
    public SpaceGamePresentation(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Latest;
        bAllowUETypesInNamespaces = true;
        MinCpuArchX64 = MinimumCpuArchitectureX64.AVX2;
        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "SpaceGameSimulation", "SandboxNative", "SGCollision", "SandboxCore", "SandboxCoreEngine", "SandboxGameShared", "SandboxISMC", "SandboxUI", "SpaceGameRendering", "CommonUI", "CommonInput", "SlateCore", "UMG", "DeveloperSettings" });
        PrivateDependencyModuleNames.AddRange(new string[] { "GameplayTags", "Niagara", "Slate", "RenderCore", "Projects", "InputCore" });
    }
}
