using System.IO;
using UnrealBuildTool;

public class GenLab : ModuleRules
{
    public GenLab(ReadOnlyTargetRules Target) : base(Target)
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
                "GenLab's native image library requires Win64 x64 with the dynamic release CRT.");
        }

        string repositoryRoot = Path.GetFullPath(Path.Combine(ModuleDirectory, "..", "..", "..", ".."));
        string nativeImageRoot = Path.Combine(repositoryRoot, "native", "image");
        string nativeImageLibrary = Path.Combine(
            repositoryRoot,
            "Binaries",
            "Native",
            "Image",
            Target.Platform.ToString(),
            Target.Configuration.ToString(),
            "sandbox_image.lib");
        PublicSystemIncludePaths.Add(Path.Combine(nativeImageRoot, "include"));
        if (File.Exists(nativeImageLibrary))
        {
            PublicAdditionalLibraries.Add(nativeImageLibrary);
        }
        else
        {
            PublicSystemLibraryPaths.Add(Path.GetDirectoryName(nativeImageLibrary)!);
            PublicSystemLibraries.Add(Path.GetFileName(nativeImageLibrary));
        }

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "AssetTools",
            "Engine",
            "ImageCore",
            "InputCore",
            "LevelEditor",
            "Projects",
            "PropertyEditor",
            "SandboxImages",
            "Slate",
            "SlateCore",
            "ToolMenus",
            "UnrealEd",
            "WorkspaceMenuStructure",
        });
    }
}
