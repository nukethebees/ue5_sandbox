#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "UObject/Interface.h"

#include "CollisionEffectComponent.generated.h"

// Use CollisionEffectHelpers::register_with_collision_system() for registration
// Supports both UActorComponent* and AActor* overloads

UINTERFACE(MinimalAPI)
class UCollisionEffectComponent : public UInterface {
    GENERATED_BODY()
};

class SANDBOX_API ICollisionEffectComponent {
    GENERATED_BODY()
  public:
    virtual int32 get_execution_priority() const { return 0; }

    virtual void execute_effect(AActor* other_actor) = 0;
};
