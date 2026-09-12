// Copyright Epic Games, Inc. All Rights Reserved.

#include "USFLoader.h"

#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "ShaderCore.h"

DEFINE_LOG_CATEGORY(LogUSFLoader);

void FUSFLoaderModule::StartupModule() {
    auto const plugin{IPluginManager::Get().FindPlugin(TEXT("USFLoader"))};
    if (!plugin.IsValid()) {
        UE_LOG(LogUSFLoader, Error, TEXT("Unable to find the USFLoader plugin."));
        return;
    }

    auto const shader_directory{FPaths::Combine(plugin->GetBaseDir(), TEXT("Shaders"))};
    AddShaderSourceDirectoryMapping(TEXT("/Plugin/USFLoader"), shader_directory);
}

IMPLEMENT_MODULE(FUSFLoaderModule, USFLoader)
