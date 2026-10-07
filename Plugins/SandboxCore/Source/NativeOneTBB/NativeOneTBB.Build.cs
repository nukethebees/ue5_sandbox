using System;
using System.IO;
using UnrealBuildTool;

public class NativeOneTBB : ModuleRules
{
    public NativeOneTBB(ReadOnlyTargetRules Target) : base(Target)
    {
        Type = ModuleType.External;

        if (Target.Platform != UnrealTargetPlatform.Win64 ||
            Target.Architecture != UnrealArch.X64 ||
            Target.bUseStaticCRT ||
            (Target.Configuration == UnrealTargetConfiguration.Debug &&
             Target.bDebugBuildsActuallyUseDebugCRT))
        {
            throw new BuildException("NativeOneTBB requires Win64 x64 with the dynamic release CRT.");
        }

        string repositoryRoot = Path.GetFullPath(
            Path.Combine(ModuleDirectory, "..", "..", "..", ".."));
        string nativeToolchain =
            Environment.GetEnvironmentVariable("IOJ_NATIVE_TOOLCHAIN") ?? "clang-cl";
        PublicSystemIncludePaths.Add(
            Path.Combine(repositoryRoot, "native", "third_party", "oneTBB", "include"));
        bool debugTbb = Target.Configuration == UnrealTargetConfiguration.Debug ||
                        Target.Configuration == UnrealTargetConfiguration.DebugGame;
        PublicDefinitions.Add("TBB_USE_DEBUG=" + (debugTbb ? "1" : "0"));
        PublicDefinitions.Add("__TBB_NO_IMPLICIT_LINKAGE=1");

        if (!Target.bGenerateProjectFiles)
        {
            string directory = Path.Combine(
                repositoryRoot, "Binaries", "Native", "OneTBB", nativeToolchain,
                Target.Platform.ToString(), Target.Configuration.ToString());
            string name = debugTbb ? "SandboxTBB12_debug" : "SandboxTBB12";
            string library = Path.Combine(directory, name + ".lib");
            string runtime = Path.Combine(directory, name + ".dll");
            if (!File.Exists(library) || !File.Exists(runtime))
            {
                throw new BuildException(
                    "NativeOneTBB expected the CMake-built library and runtime under '{0}'. " +
                    "Build Unreal targets through a repository CMake workflow.", directory);
            }

            PublicAdditionalLibraries.Add(library);
            RuntimeDependencies.Add("$(BinaryOutputDir)/" + name + ".dll", runtime);
            ExternalDependencies.Add(library);
            ExternalDependencies.Add(runtime);
        }
    }
}
