#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "Sandbox/players/npcs/data/CombatProfile.h"

#include "CombatActor.generated.h"

UINTERFACE(MinimalAPI)
class UCombatActor : public UInterface {
    GENERATED_BODY()
};

class SANDBOX_API ICombatActor {
    GENERATED_BODY()
  public:
    virtual bool attack_actor(AActor& target) = 0;
    virtual auto get_combat_profile() const -> FCombatProfile = 0;
};
