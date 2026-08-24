#pragma once

#include "Modules/ModuleManager.h"

class FSandboxUIModule : public IModuleInterface {
  public:
    void StartupModule() override;
    void ShutdownModule() override;
};
