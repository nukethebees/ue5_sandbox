using UnrealBuildTool;

public class SpaceGameS7 : ModuleRules
{
    public SpaceGameS7(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Latest;
        bAllowUETypesInNamespaces = true;
        MinCpuArchX64 = MinimumCpuArchitectureX64.AVX2;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "CommonUI",
            "Core",
            "CoreUObject",
            "SpaceGame",
            "SpaceGameSimulation",
            "SpaceGamePresentation",
            "UMG",
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Engine",
            "InputCore",
            "NativeLevelAuthoring",
            "PlatformCrypto",
            "PlatformCryptoContext",
            "Slate",
            "SlateCore",
        });

        RuntimeDependencies.Add(
            "$(ProjectDir)/LevelScripts/...*.scm",
            StagedFileType.UFS);
    }
}
