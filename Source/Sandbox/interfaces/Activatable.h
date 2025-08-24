
#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Activatable.generated.h"

UINTERFACE(MinimalAPI)
class UActivatable : public UInterface {
    GENERATED_BODY()
};

class SANDBOX_API IActivatable {
    GENERATED_BODY()
  public:
    virtual void activate(AActor* instigator = nullptr) = 0;
    virtual bool can_activate(AActor const* instigator) const { return true; }

    template <typename T, typename U>
    static bool try_activate(T* target, U* instigator) {
        if (target->GetClass()->ImplementsInterface(UActivatable::StaticClass())) {
            auto const activatable{TScriptInterface<IActivatable>(target)};
            if (activatable->can_activate(instigator)) {
                activatable->activate(instigator);
                return true;
            }
        }
        return false;
    }
};
