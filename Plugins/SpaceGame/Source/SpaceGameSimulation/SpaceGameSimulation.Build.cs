using UnrealBuildTool;

public class SpaceGameSimulation : ModuleRules
{
    public SpaceGameSimulation(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Latest;
        bAllowUETypesInNamespaces = true;
        MinCpuArchX64 = MinimumCpuArchitectureX64.AVX2;
        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "NativeMemory", "SandboxCore", "SandboxCoreEngine", "SandboxNative", "SGCollision" });
        PrivateDependencyModuleNames.AddRange(new string[] { "TraceLog" });
    }
}
