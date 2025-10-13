#pragma once

#include "CoreMinimal.h"
#include "EngineUtils.h"

namespace ml {
template <typename T>
T* get_first_actor(UWorld& world) {
    for (auto it{TActorIterator<T>(&world)}; it; ++it) {
        return *it;
    }
    return nullptr;
}
}
