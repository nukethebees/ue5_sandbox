using UnrealBuildTool;

[SupportedPlatforms("Win64")]
[SupportedConfigurations(UnrealTargetConfiguration.Development)]
public class SandboxEmbeddedTarget : SandboxTarget
{
    public SandboxEmbeddedTarget(TargetInfo target) : base(target)
    {
        BuildEnvironment = TargetBuildEnvironment.Unique;
        LinkType = TargetLinkType.Monolithic;
        bShouldCompileAsDLL = true;
        GlobalDefinitions.Add("UE_LIBRARY_ENABLED=1");
        ExtraModuleNames.Add("UELibrary");
    }
}
