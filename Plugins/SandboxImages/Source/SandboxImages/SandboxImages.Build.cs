using UnrealBuildTool;

public class SandboxImages : ModuleRules
{
    public SandboxImages(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Latest;

        PublicDependencyModuleNames.Add("Core");
    }
}
