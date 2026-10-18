using UnrealBuildTool;

public class NativeSettings : ModuleRules
{
    public NativeSettings(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Latest;
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "NativeLevels",
        });
        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "NativeLevelAuthoring",
            "PlatformCrypto",
            "PlatformCryptoContext",
        });

        RuntimeDependencies.Add(
            "$(ProjectDir)/LevelScripts/...*.scm",
            StagedFileType.NonUFS);
    }
}
