using UnrealBuildTool;
using System.IO;

public class SandboxEditor : ModuleRules
{
    public SandboxEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Latest;
        MinCpuArchX64 = MinimumCpuArchitectureX64.AVX2;
        PublicIncludePaths.Add(ModuleDirectory);

        if (Target.Platform != UnrealTargetPlatform.Win64 ||
            Target.Architecture != UnrealArch.X64 ||
            Target.bUseStaticCRT ||
            (Target.Configuration == UnrealTargetConfiguration.Debug &&
             Target.bDebugBuildsActuallyUseDebugCRT))
        {
            throw new BuildException(
                "MaterialGen requires Win64 x64 with the dynamic release CRT.");
        }

        string repositoryRoot = Path.GetFullPath(Path.Combine(ModuleDirectory, "..", ".."));
        string nativeMaterialGenRoot = Path.Combine(repositoryRoot, "native", "lispb", "material");
        string nativeMaterialGenLibrary = Path.Combine(
            repositoryRoot,
            "Binaries",
            "Native",
            "MaterialGen",
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

        // Core dependencies
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "SandboxCore",
            "Core",
            "CoreUObject",
            "Engine",
            "UMG",
            "UMGEditor",
            "UnrealEd", // Must be public for class inheritance
            "EditorFramework" // Required by UnrealEd, must be public
        });

        // Editor-specific dependencies
        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "AssetRegistry", // For scanning data assets
            "AssetTools",
            "AudioEditor",
            "BlueprintEditorLibrary",
            "BlueprintGraph", // Required by UnrealEd
            "CommonUI",
            "EditorSubsystem",
            "EditorStyle",
            "EditorWidgets",
            "EnhancedInput",
            "EngineSettings", // Engine configuration access
            "GraphEditor",
            "MaterialEditor",
            "PropertyEditor",
            "RenderCore", // Often needed for material nodes
            "RHI",
            "Sandbox", // Reference to runtime module
            "SandboxGameShared",
            "SandboxUI",
            "SbxShadersExperiments",
            "SbxMeshGenLab",
            "SpaceGame",
            "SpaceGameSimulation",
            "SpaceGamePresentation",
            "SpaceGameS7",
            "Slate",
            "SlateCore",
            "SlateRHIRenderer",
            "InputCore", // For SNumericVectorInputBox 
            "ImageCore",
            "ToolWidgets",
            "ToolMenus", // For editor toolbar buttons
            "USFLoaderEditor" // USF Loader plugin dependency            
        });
    }
}
