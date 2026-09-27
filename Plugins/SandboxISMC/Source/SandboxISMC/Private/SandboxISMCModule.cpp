#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "ShaderCore.h"

class FSandboxISMCModule final : public IModuleInterface {
  public:
    void StartupModule() override {
        auto const plugin{IPluginManager::Get().FindPlugin(TEXT("SandboxISMC"))};
        check(plugin.IsValid());
        AddShaderSourceDirectoryMapping(TEXT("/SandboxISMC"),
                                        FPaths::Combine(plugin->GetBaseDir(), TEXT("Shaders")));
    }
};

IMPLEMENT_MODULE(FSandboxISMCModule, SandboxISMC)
