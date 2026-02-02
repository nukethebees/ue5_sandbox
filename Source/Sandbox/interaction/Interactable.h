// IDescribable.h
#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "Interactable.generated.h"

class AActor;

UINTERFACE(BlueprintType)
class UInteractable : public UInterface {
    GENERATED_BODY()
};

class IInteractable {
    GENERATED_BODY()
  public:
    virtual void on_interacted(AActor& instigator) = 0;
};
