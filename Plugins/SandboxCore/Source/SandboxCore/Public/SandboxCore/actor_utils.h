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
}
