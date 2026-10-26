using UnrealBuildTool;
using System;
using System.IO;

public class SandboxISMC : ModuleRules
{
    public SandboxISMC(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivatePCHHeaderFile = "Private/SandboxISMCPCH.h";
        CppStandard = CppStandardVersion.Latest;
        MinCpuArchX64 = MinimumCpuArchitectureX64.AVX2;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "SandboxCore",
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Projects",
            "RenderCore",
            "RHI",
        });
        string repositoryRoot = Path.GetFullPath(Path.Combine(ModuleDirectory, "..", "..", "..", ".."));
        PublicSystemIncludePaths.Add(Path.Combine(repositoryRoot, "native", "ismc", "lib", "include"));
        if (!Target.bGenerateProjectFiles)
        {
            string nativeToolchain = Environment.GetEnvironmentVariable("IOJ_NATIVE_TOOLCHAIN") ?? "clang-cl";
            string library = Path.Combine(repositoryRoot, "Binaries", "Native", "ISMC", nativeToolchain,
                Target.Platform.ToString(), Target.Configuration.ToString(), "SandboxNativeISMC.lib");
            string highwayLibrary = Path.Combine(repositoryRoot, "Binaries", "Native", "Highway", nativeToolchain,
                Target.Platform.ToString(), Target.Configuration.ToString(), "hwy.lib");
            foreach (string dependency in new[] { library, highwayLibrary })
            {
                if (!File.Exists(dependency))
                {
                    throw new BuildException("SandboxISMC expected '{0}'. Build through a repository CMake workflow.", dependency);
                }
                PublicAdditionalLibraries.Add(dependency);
                ExternalDependencies.Add(dependency);
            }
        }
    }
}
