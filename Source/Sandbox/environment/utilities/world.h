#pragma once

#include "CoreMinimal.h"
#include "Engine/World.h"
#include "EngineUtils.h"

namespace ml {
template <typename T>
T* get_first_actor(UWorld& world) {
    for (auto it{TActorIterator<T>(&world)}; it; ++it) {
        return *it;
    }
    return nullptr;
}

template <typename T>
T* get_or_create_actor_singleton(UWorld& world) {
    if (auto* actor{ml::get_first_actor<T>(world)}) {
        return actor;
    }

    FActorSpawnParameters spawn_params{};
    auto* actor{world.SpawnActor<T>(FVector::ZeroVector, FRotator::ZeroRotator, spawn_params)};

    if (actor) {
        actor->SetActorLabel(T::StaticClass()->GetName());
    }

    return actor;
}
}
