#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "MovementMultiplierReceiver.generated.h"

UINTERFACE(MinimalAPI)
class UMovementMultiplierReceiver : public UInterface {
    GENERATED_BODY()
};

class SANDBOX_API IMovementMultiplierReceiver {
    GENERATED_BODY()
  public:
    UFUNCTION()
    virtual void set_movement_multiplier(float multiplier) = 0;

    template <typename T>
    static void try_set_multiplier(T* target, float multiplier) {
        if (target->GetClass()->ImplementsInterface(UMovementMultiplierReceiver::StaticClass())) {
            auto const obj{TScriptInterface<IMovementMultiplierReceiver>(target)};
            obj->set_movement_multiplier(multiplier);
        }
    }
};
