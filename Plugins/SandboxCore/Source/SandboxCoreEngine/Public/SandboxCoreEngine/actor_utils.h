#pragma once

#include "spawn_phase.h"

#include <SandboxCore/container_concepts.h>
#include <SandboxCore/log_categories.h>

#include <CoreMinimal.h>
#include <Engine/World.h>
#include <EngineUtils.h>

#include <concepts>

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

namespace detail {
template <typename>
inline constexpr bool always_false{false};

template <typename T, typename Fn>
void invoke_spawn_actors_callback(Fn& fn, TArrayView<T*> actors, ESpawnPhase const phase) {
    if constexpr (std::invocable<Fn&, TArrayView<T*>, ESpawnPhase>) {
        fn(actors, phase);
    } else if constexpr (std::invocable<Fn&, T&, int32, ESpawnPhase>) {
        auto const actor_count{actors.Num()};
        for (auto actor_index{0}; actor_index < actor_count; ++actor_index) {
            fn(*actors[actor_index], actor_index, phase);
        }
    } else {
        static_assert(always_false<Fn>,
                      "spawn_actors callback must be invocable as either "
                      "void(TArrayView<T*>, ESpawnPhase) or void(T&, int32, ESpawnPhase).");
    }
}
}

template <typename T,
          HasMutableNumAndGetData Container,
          typename Fn = decltype([](TArrayView<T*>, ESpawnPhase) {})>
void spawn_actors(UWorld& world, UClass* actor_class, Container&& out_actors, Fn&& fn = {}) {
    if (!actor_class) {
        UE_LOG(LogSandboxCore, Fatal, TEXT("spawn_actors: actor_class is nullptr."));
        return;
    }

    TArrayView<T*> actors_view{out_actors.GetData(), out_actors.Num()};

    auto const actor_count{actors_view.Num()};
    for (auto actor_index{0}; actor_index < actor_count; ++actor_index) {
        auto* const actor{world.SpawnActorDeferred<T>(actor_class, FTransform::Identity)};
        if (!actor) {
            UE_LOG(LogSandboxCore,
                   Fatal,
                   TEXT("spawn_actors: failed to deferred-spawn actor class '%s'."),
                   *actor_class->GetName());
            return;
        }

        actors_view[actor_index] = actor;
    }

    detail::invoke_spawn_actors_callback<T>(fn, actors_view, ESpawnPhase::PreSpawn);

    for (auto actor_index{0}; actor_index < actor_count; ++actor_index) {
        actors_view[actor_index]->FinishSpawning(FTransform::Identity);
    }

    detail::invoke_spawn_actors_callback<T>(fn, actors_view, ESpawnPhase::PostSpawn);
}
template <typename T,
          HasMutableNumAndGetData Container,
          typename Fn = decltype([](TArrayView<T*>, ESpawnPhase) {})>
void spawn_actors(UWorld& world, Container&& out_actors, Fn&& fn = {}) {
    spawn_actors<T, Container, Fn>(
        world, T::StaticClass(), std::forward<Container>(out_actors), std::forward<Fn>(fn));
}
template <typename T, int32 N, typename Fn = decltype([](TArrayView<T*>, ESpawnPhase) {})>
void spawn_actors(UWorld& world, Fn&& fn = {}) {
    TStaticArray<T*, N> actors;
    spawn_actors<T, TArrayView<T*>, Fn>(
        world, T::StaticClass(), TArrayView<T*>{actors.GetData(), N}, std::forward<Fn>(fn));
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
