
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
};
