// Copyright Epic Games, Inc. All Rights Reserved.

using Microsoft.Extensions.Logging;
using System.IO;
using UnrealBuildTool;

public class SandboxCore : ModuleRules
{
    public SandboxCore(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Latest;
        MinCpuArchX64 = MinimumCpuArchitectureX64.AVX2;
        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
            }
            );

        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            Target.Logger.LogInformation($"Target.WindowsPlatform.ToolChainDir : {Target.WindowsPlatform.ToolChainDir}");
            Target.Logger.LogInformation($"Target.WindowsPlatform.WindowsSdkDir : {Target.WindowsPlatform.WindowsSdkDir}");
            Target.Logger.LogInformation($"Target.WindowsPlatform.WindowsSdkVersion : {Target.WindowsPlatform.WindowsSdkVersion}");

            string OneCoreLib = Path.Combine(Target.WindowsPlatform.WindowsSdkDir,
                "Lib",
                Target.WindowsPlatform.WindowsSdkVersion,
                "um",
                "x64",
                "OneCore.lib"
            );

            PublicAdditionalLibraries.Add(OneCoreLib);
        }
    }
}
