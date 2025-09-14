// Sandbox.h
#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

#if WITH_EDITOR
class FUSFLoaderNodeFactory;
#endif

class FSandboxModule : public IModuleInterface {
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

#if WITH_EDITOR
private:
    /** Factory for creating custom USF Loader graph nodes */
    TSharedPtr<FUSFLoaderNodeFactory> USFLoaderNodeFactory;
#endif
};