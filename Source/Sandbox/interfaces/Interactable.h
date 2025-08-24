#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Interactable.generated.h"

UINTERFACE(MinimalAPI)
class UInteractable : public UInterface {
    GENERATED_BODY()
};

class SANDBOX_API IInteractable {
    GENERATED_BODY()
  public:
    virtual void interact(AActor* interactor = nullptr) = 0;
    virtual bool can_interact(AActor const* interactor) const { return true; }

    template <typename T, typename U>
    static bool try_interact(T* target, U* instigator) {
        if (target->GetClass()->ImplementsInterface(UInteractable::StaticClass())) {
            auto const interactable{TScriptInterface<IInteractable>(target)};
            if (interactable->can_interact(instigator)) {
                interactable->interact(instigator);
                return true;
            }
        }
        return false;
    }
};
