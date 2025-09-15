#include "USFLoaderEditor.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "ShaderCore.h"

#define LOCTEXT_NAMESPACE "FUSFLoaderEditorModule"

void FUSFLoaderEditorModule::StartupModule() {
    // Register virtual shader directory mapping for the plugin
    FString PluginShaderDir = FPaths::Combine(
        IPluginManager::Get().FindPlugin(TEXT("USFLoader"))->GetBaseDir(), TEXT("Shaders"));
    AddShaderSourceDirectoryMapping(TEXT("/Plugin/USFLoader"), PluginShaderDir);
}

void FUSFLoaderEditorModule::ShutdownModule() {
    // This function may be called during shutdown to clean up your module
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FUSFLoaderEditorModule, USFLoaderEditor)
