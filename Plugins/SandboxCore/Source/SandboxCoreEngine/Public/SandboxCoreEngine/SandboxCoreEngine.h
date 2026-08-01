#pragma once

#include "Modules/ModuleManager.h"

class FSandboxCoreEngineModule : public IModuleInterface {
  public:
    void StartupModule() override;
    void ShutdownModule() override;
};
