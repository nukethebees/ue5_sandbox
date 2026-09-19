#include "Modules/ModuleManager.h"

#include <CQTest.h>

TEST_CLASS(SandboxISMCModuleLoad, "SandboxISMC.ModuleLoadSmoke")
{
    TEST_METHOD(LoadsAllPluginModules)
    {
        auto& module_manager{FModuleManager::Get()};
        TestRunner->TestTrue(TEXT("SandboxISMC runtime module is loaded"),
                             module_manager.IsModuleLoaded(TEXT("SandboxISMC")));
        TestRunner->TestTrue(TEXT("SandboxISMC lab module is loaded"),
                             module_manager.IsModuleLoaded(TEXT("SandboxISMCLab")));
        TestRunner->TestTrue(TEXT("SandboxISMC test module is loaded"),
                             module_manager.IsModuleLoaded(TEXT("SandboxISMCTests")));
    }
};
