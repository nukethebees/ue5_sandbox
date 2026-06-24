#pragma once

#include "CoreMinimal.h"
#include "Delegates/IDelegateInstance.h"
#include "Modules/ModuleManager.h"

class FSandboxTestsModule : public IModuleInterface {
  public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
};
