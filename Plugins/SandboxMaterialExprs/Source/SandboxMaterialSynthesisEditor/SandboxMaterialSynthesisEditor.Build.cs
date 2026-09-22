using System;
using System.IO;
using UnrealBuildTool;

public class SandboxMaterialSynthesisEditor : ModuleRules
{
    public SandboxMaterialSynthesisEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Latest;
        MinCpuArchX64 = MinimumCpuArchitectureX64.AVX2;

        if (Target.Platform != UnrealTargetPlatform.Win64 ||
            Target.Architecture != UnrealArch.X64 ||
            Target.bUseStaticCRT ||
            (Target.Configuration == UnrealTargetConfiguration.Debug &&
             Target.bDebugBuildsActuallyUseDebugCRT))
        {
            throw new BuildException(
                "MaterialGen requires Win64 x64 with the dynamic release CRT.");
        }

        string repositoryRoot = Path.GetFullPath(Path.Combine(ModuleDirectory, "..", "..", "..", ".."));
        string nativeToolchain = Environment.GetEnvironmentVariable("SANDBOX_NATIVE_TOOLCHAIN") ?? "clang-cl";
        string nativeMaterialGenRoot = Path.Combine(repositoryRoot, "native", "lispb", "material");
        string nativeMaterialGenLibrary = Path.Combine(
            repositoryRoot,
            "Binaries",
            "Native",
            "MaterialGen",
            nativeToolchain,
            Target.Platform.ToString(),
            Target.Configuration.ToString(),
            "sandbox_material_gen.lib");
        PublicSystemIncludePaths.Add(Path.Combine(nativeMaterialGenRoot, "lib", "include"));
        if (File.Exists(nativeMaterialGenLibrary))
        {
            PublicAdditionalLibraries.Add(nativeMaterialGenLibrary);
        }
        else
        {
            PublicSystemLibraryPaths.Add(Path.GetDirectoryName(nativeMaterialGenLibrary)!);
            PublicSystemLibraries.Add(Path.GetFileName(nativeMaterialGenLibrary));
        }

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "AssetRegistry",
            "Core",
            "CoreUObject",
            "CQTest",
            "Engine",
            "MaterialEditor",
            "RenderCore",
            "RHI",
            "UnrealEd",
        });
    }
}
