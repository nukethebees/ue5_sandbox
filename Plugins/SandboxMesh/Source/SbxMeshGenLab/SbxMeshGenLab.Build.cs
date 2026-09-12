using System.IO;
using UnrealBuildTool;

public class SbxMeshGenLab : ModuleRules
{
    public SbxMeshGenLab(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Latest;

        if (Target.Platform != UnrealTargetPlatform.Win64 ||
            Target.Architecture != UnrealArch.X64 ||
            Target.bUseStaticCRT ||
            (Target.Configuration == UnrealTargetConfiguration.Debug &&
             Target.bDebugBuildsActuallyUseDebugCRT))
        {
            throw new BuildException(
                "SbxMeshGenLab's native mesh library requires Win64 x64 with the dynamic release CRT.");
        }

        string repositoryRoot = Path.GetFullPath(Path.Combine(ModuleDirectory, "..", "..", "..", ".."));
        string nativeMeshRoot = Path.Combine(repositoryRoot, "native", "mesh_gen");
        string nativeMeshLibrary = Path.Combine(
            repositoryRoot,
            "Binaries",
            "Native",
            "MeshGen",
            Target.Platform.ToString(),
            Target.Configuration.ToString(),
            "sandbox_mesh_gen.lib");
        PublicSystemIncludePaths.Add(Path.Combine(nativeMeshRoot, "lib", "include"));
        if (File.Exists(nativeMeshLibrary))
        {
            PublicAdditionalLibraries.Add(nativeMeshLibrary);
        }
        else
        {
            PublicSystemLibraryPaths.Add(Path.GetDirectoryName(nativeMeshLibrary)!);
            PublicSystemLibraries.Add(Path.GetFileName(nativeMeshLibrary));
        }

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "AssetRegistry",
            "Blutility",
            "CQTest",
            "DesktopPlatform",
            "EditorFramework",
            "Engine",
            "InputCore",
            "Json",
            "JsonUtilities",
            "MeshDescription",
            "PropertyEditor",
            "Projects",
            "Slate",
            "SlateCore",
            "StaticMeshDescription",
            "ToolMenus",
            "UMG",
            "UnrealEd",
            "WorkspaceMenuStructure",
        });
    }
}
