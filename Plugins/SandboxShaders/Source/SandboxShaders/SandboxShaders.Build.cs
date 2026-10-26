using UnrealBuildTool;
using System.IO;

public class SandboxShaders : ModuleRules
{
    public SandboxShaders(ReadOnlyTargetRules Target) : base(Target)
    {
        string repositoryRoot = Path.GetFullPath(Path.Combine(ModuleDirectory, "..", "..", "..", ".."));
        PublicSystemIncludePaths.Add(Path.Combine(repositoryRoot, "native", "gfx", "lib", "include"));
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivatePCHHeaderFile = "Private/SandboxShadersPCH.h";
        CppStandard = CppStandardVersion.Latest;
        MinCpuArchX64 = MinimumCpuArchitectureX64.AVX2;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "SandboxCore",
            "NativeLevels",
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Projects",
            "NativeLevelAuthoring",
            "RenderCore",
            "RHI",
        });
    }
}
