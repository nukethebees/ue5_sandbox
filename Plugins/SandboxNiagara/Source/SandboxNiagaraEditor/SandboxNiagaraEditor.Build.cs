using UnrealBuildTool;

public class SandboxNiagaraEditor : ModuleRules
{
    public SandboxNiagaraEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Latest;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "EditorSubsystem",
            "UnrealEd",
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Engine",
            "Niagara",
            "NiagaraEditor",
        });
    }
}
