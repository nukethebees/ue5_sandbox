#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "MaxSpeedChangeListener.generated.h"

UINTERFACE(MinimalAPI)
class UMaxSpeedChangeListener : public UInterface {
    GENERATED_BODY()
};

class SANDBOX_API IMaxSpeedChangeListener {
    GENERATED_BODY()
  public:
    UFUNCTION()
    virtual void on_speed_changed(float new_speed) = 0;

    template <typename T>
    static void try_broadcast(T* target, float val) {
        if (target->GetClass()->ImplementsInterface(UMaxSpeedChangeListener::StaticClass())) {
            auto const obj{TScriptInterface<IMaxSpeedChangeListener>(target)};
            obj->on_speed_changed(val);
        }
    }
};
