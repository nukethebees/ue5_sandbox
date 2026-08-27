#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "ShaderCore.h"

class FSandboxShadersModule final : public IModuleInterface {
  public:
    void StartupModule() override {
        auto const plugin{IPluginManager::Get().FindPlugin(TEXT("SandboxShaders"))};
        check(plugin.IsValid());

        auto const shader_directory{FPaths::Combine(plugin->GetBaseDir(), TEXT("Shaders"))};
        AddShaderSourceDirectoryMapping(TEXT("/Plugin/SandboxShaders"), shader_directory);
    }
};

IMPLEMENT_MODULE(FSandboxShadersModule, SandboxShaders)
