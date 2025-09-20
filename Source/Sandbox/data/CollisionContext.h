#pragma once

#include "CoreMinimal.h"

class UWorld;
class AActor;

struct FCollisionContext {
    UWorld& world;
    AActor& collided_actor;

    FCollisionContext() = delete;
    FCollisionContext(UWorld& world, AActor& collided_actor)
        : world(world)
        , collided_actor(collided_actor) {}
};
