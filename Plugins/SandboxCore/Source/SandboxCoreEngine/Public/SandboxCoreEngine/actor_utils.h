#pragma once

#include <SandboxCore/log_categories.h>

#include <CoreMinimal.h>
#include <Engine/World.h>
#include <EngineUtils.h>

namespace ml {
template <typename T>
auto get_first_actor(UWorld& world) -> T* {
    for (TActorIterator<T> it{&world}; it; ++it) {
        return *it;
    }

    return nullptr;
}

template <typename T>
void get_first_actor(UWorld& world, T*& actor) {
    if (auto* const first_actor{get_first_actor<T>(world)}) {
        actor = first_actor;
    }
}

template <typename T>
auto get_or_create_actor_singleton(UWorld& world) -> T* {
    if (auto* const actor{get_first_actor<T>(world)}) {
        return actor;
    }

    FActorSpawnParameters spawn_params{};
    auto* const actor{
        world.SpawnActor<T>(FVector::ZeroVector, FRotator::ZeroRotator, spawn_params)};

    if (actor) {
        actor->SetActorLabel(T::StaticClass()->GetName());
    }

    return actor;
}

template <typename TActor, typename F>
void for_each_instance(TActor& actor, F&& fn) {
    auto* world{actor.GetWorld()};
    if (!world) {
        UE_LOG(LogSandboxCore, Warning, TEXT("for_each_instance: world is nullptr."));
        return;
    }

    for (auto it{TActorIterator<TActor>{world}}; it; ++it) {
        auto* instance{*it};
        if (!IsValid(instance)) {
            continue;
        }

        fn(*instance);
    }
}

template <typename T>
auto append_actors(UWorld const& world, TArray<T*>& out_actors) -> void {
    // TActorIterator skips actors pending kill automatically
    for (TActorIterator<T> it{&world}; it; ++it) {
        out_actors.Add(*it);
    }
}

template <typename T>
auto get_actors(UWorld const& world) -> TArray<T*> {
    TArray<T*> out_actors;
    append_actors(world, out_actors);
    return out_actors;
}

template <typename T>
auto count_actors(UWorld const& world) -> int32 {
    auto count{int32{0}};
    for (TActorIterator<T> it{&world}; it; ++it) {
        ++count;
    }

    return count;
}
}
