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
        if (Target.Platform != UnrealTargetPlatform.Win64 || Target.Architecture != UnrealArch.X64 || Target.bUseStaticCRT)
        {
            throw new BuildException("SbxCoreExperiments mimalloc requires Win64 x64 with the dynamic CRT.");
        }
        string repositoryRoot = Path.GetFullPath(Path.Combine(ModuleDirectory, "..", "..", "..", ".."));
        string packageRoot = Path.Combine(repositoryRoot, "vcpkg_installed", "x64-windows");
        bool debugCrt = Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT;
        string variantRoot = debugCrt ? Path.Combine(packageRoot, "debug") : packageRoot;
        string dllName = debugCrt ? "mimalloc-debug.dll" : "mimalloc.dll";
        string dll = Path.Combine(variantRoot, "bin", dllName);
        if (!File.Exists(dll))
        {
            throw new BuildException("Configure an Unreal CMake preset to provision mimalloc: missing {0}", dll);
        }
        PrivateIncludePaths.Add(Path.Combine(packageRoot, "include"));
        RuntimeDependencies.Add("$(TargetOutputDir)/sbx-mimalloc.dll", dll);
        RuntimeDependencies.Add("$(TargetOutputDir)/mimalloc-redirect.dll", Path.Combine(variantRoot, "bin", "mimalloc-redirect.dll"));
    }
}