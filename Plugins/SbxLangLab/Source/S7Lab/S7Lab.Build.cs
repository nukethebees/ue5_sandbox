using UnrealBuildTool;

public class S7Lab : ModuleRules
{
    public S7Lab(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.NoPCHs;
        CppStandard = CppStandardVersion.Latest;
        bUseUnity = false;
        CppCompileWarningSettings.ShadowVariableWarningLevel = WarningLevel.Off;
        CppCompileWarningSettings.UndefinedIdentifierWarningLevel = WarningLevel.Off;
        CppCompileWarningSettings.UnreachableCodeWarningLevel = WarningLevel.Off;

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Core",
        });

        PrivateDefinitions.AddRange(new string[]
        {
            "HAVE_COMPLEX_NUMBERS=0",
            "HAVE_COMPLEX_TRIG=0",
            "WITH_C_LOADER=0",
            "WITH_GMP=0",
            "WITH_MAIN=0",
            "WITH_SYSTEM_EXTRAS=0",
        });

        PrivateIncludePaths.Add("S7Lab/Private/ThirdParty/s7");
    }
}
