#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "ShaderCore.h"

class FSpaceGameRenderingModule final : public IModuleInterface {
  public:
    void StartupModule() override {
        auto const plugin{IPluginManager::Get().FindPlugin(TEXT("SpaceGame"))};
        check(plugin.IsValid());

        auto const shader_directory{FPaths::Combine(plugin->GetBaseDir(), TEXT("Shaders"))};
        AddShaderSourceDirectoryMapping(TEXT("/Plugin/SpaceGame"), shader_directory);
    }
};

IMPLEMENT_MODULE(FSpaceGameRenderingModule, SpaceGameRendering)
