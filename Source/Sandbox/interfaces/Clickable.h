#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Clickable.generated.h"

UINTERFACE(MinimalAPI)
class UClickable : public UInterface {
    GENERATED_BODY()
};

class SANDBOX_API IClickable {
    GENERATED_BODY()
  public:
    virtual void OnClicked() = 0;

    template <typename T>
    static void try_click(T* ptr) {
        if (!ptr) {
            return;
        }

        if (ptr->GetClass()->ImplementsInterface(UClickable::StaticClass())) {
            if (auto* clickable{Cast<IClickable>(ptr)}) {
                clickable->OnClicked();
            }
        }
    }
};
