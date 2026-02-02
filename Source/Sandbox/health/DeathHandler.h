#pragma once

#include "Sandbox/logging/SandboxLogCategories.h"

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "DeathHandler.generated.h"

UINTERFACE(MinimalAPI)
class UDeathHandler : public UInterface {
    GENERATED_BODY()
};

class SANDBOX_API IDeathHandler {
    GENERATED_BODY()
  public:
    virtual void handle_death() = 0;

    static bool killable(auto* target) {
        return target->GetClass()->ImplementsInterface(UDeathHandler::StaticClass());
    }

    template <typename T>
    static bool kill(T* target) {
        if (killable(target)) {
            auto const dying{TScriptInterface<IDeathHandler>(target)};
            dying->handle_death();
            return true;
        }
        UE_LOG(LogSandboxHealth, Warning, TEXT("Target does not implement a death handler."));
        return false;
    }
    template <typename T>
    static bool try_kill(T* target) {
        if (killable(target)) {
            auto const dying{TScriptInterface<IDeathHandler>(target)};
            dying->handle_death();
            return true;
        }
        return false;
    }
};
