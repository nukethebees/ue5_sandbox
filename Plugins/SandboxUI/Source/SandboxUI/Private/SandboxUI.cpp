#include "SandboxUI/SandboxUI.h"

#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "ShaderCore.h"

void FSandboxUIModule::StartupModule() {
    auto const plugin{IPluginManager::Get().FindPlugin(TEXT("SandboxUI"))};
    check(plugin.IsValid());

    auto const shader_directory{FPaths::Combine(plugin->GetBaseDir(), TEXT("Shaders"))};
    AddShaderSourceDirectoryMapping(TEXT("/Plugin/SandboxUI"), shader_directory);
}

void FSandboxUIModule::ShutdownModule() {}

IMPLEMENT_MODULE(FSandboxUIModule, SandboxUI)
