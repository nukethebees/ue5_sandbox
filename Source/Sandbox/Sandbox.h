// Sandbox.h
#pragma once

#include "CoreMinimal.h"

#include "Modules/ModuleManager.h"

class FSandboxModule : public IModuleInterface {
  public:
    void StartupModule() override;
    void ShutdownModule() override;
};
