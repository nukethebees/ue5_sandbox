using UnrealBuildTool;
using System.IO;

public class SbxCoreExperiments : ModuleRules
{
    public SbxCoreExperiments(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Latest;
        MinCpuArchX64 = MinimumCpuArchitectureX64.AVX2;
        FPSemantics = FPSemanticsMode.Precise;
        bAllowUETypesInNamespaces = true;
        PublicDependencyModuleNames.AddRange(new string[] { "Core", "SandboxCore" });
    }
}
