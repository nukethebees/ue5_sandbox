#pragma once

#include <SandboxCore/log_categories.h>

#include <EngineUtils.h>

namespace ml {
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
}
