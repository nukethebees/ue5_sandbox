#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "CombatActor.generated.h"

UINTERFACE(MinimalAPI)
class UCombatActor : public UInterface {
    GENERATED_BODY()
};

class SANDBOX_API ICombatActor {
    GENERATED_BODY()
  public:
    virtual void attack_actor(AActor* target) = 0;
};
