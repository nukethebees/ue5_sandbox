using UnrealBuildTool;

public class LuaLab : ModuleRules
{
    public LuaLab(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.NoPCHs;
        CppStandard = CppStandardVersion.Latest;
        bUseUnity = false;

        CppCompileWarningSettings.ShadowVariableWarningLevel = WarningLevel.Off;
        CppCompileWarningSettings.UndefinedIdentifierWarningLevel = WarningLevel.Off;
        CppCompileWarningSettings.UnreachableCodeWarningLevel = WarningLevel.Off;

        PublicDependencyModuleNames.Add("Core");

        PrivateIncludePaths.Add("LuaLab/Private/ThirdParty/lua");
    }
}
