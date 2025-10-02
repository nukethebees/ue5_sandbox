#pragma once

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

    template <typename T>
    static void try_kill(T* target) {
        if (target->GetClass()->ImplementsInterface(UDeathHandler::StaticClass())) {
            auto const dying{TScriptInterface<IDeathHandler>(target)};
            dying->handle_death();
        }
    }
};
