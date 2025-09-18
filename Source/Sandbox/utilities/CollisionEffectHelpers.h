#pragma once

#include "CoreMinimal.h"

class UActorComponent;
class AActor;
class UWorld;

class SANDBOX_API CollisionEffectHelpers {
  public:
    static void register_with_collision_system(UActorComponent* component);
    static void register_with_collision_system(AActor* actor);
    static UWorld* get_world_from_component(UActorComponent* component);
    static UWorld* get_world_from_actor(AActor* actor);
};
