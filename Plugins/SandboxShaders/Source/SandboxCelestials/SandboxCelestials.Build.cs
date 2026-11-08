using UnrealBuildTool;

public class SandboxCelestials : ModuleRules
{
    public SandboxCelestials(ReadOnlyTargetRules Target) : base(Target)
    {
        bAllowUETypesInNamespaces = true;
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivatePCHHeaderFile = "Private/SandboxCelestialsPCH.h";
        CppStandard = CppStandardVersion.Latest;
        MinCpuArchX64 = MinimumCpuArchitectureX64.AVX2;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
        });

        PrivateDependencyModuleNames.Add("SandboxShaders");
    }
}
