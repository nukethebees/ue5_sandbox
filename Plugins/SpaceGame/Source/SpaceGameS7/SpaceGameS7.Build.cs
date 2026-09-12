using UnrealBuildTool;

public class SpaceGameS7 : ModuleRules
{
    public SpaceGameS7(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Latest;
        bAllowUETypesInNamespaces = true;

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
            "NativeS7",
            "PlatformCrypto",
            "PlatformCryptoContext",
            "Slate",
            "SlateCore",
        });
    }
}
