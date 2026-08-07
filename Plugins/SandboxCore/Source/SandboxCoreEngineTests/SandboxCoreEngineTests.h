#pragma once

#include "Modules/ModuleInterface.h"

class FSandboxCoreEngineTestsModule final : public IModuleInterface {
  public:
    void StartupModule() override {}
    void ShutdownModule() override {}
};
