#pragma once

#include <optional>

#include "CoreMinimal.h"

#include "Sandbox/core/object_pooling/concepts/ObjectPoolConcepts.h"

template <typename T, int32 DefaultPoolSize_, int32 MaxPoolSize_ = -1>
struct FPoolConfig {
    using ActorType = T;

    static_assert(IsPoolableActor<ActorType>);

    static constexpr int32 DefaultPoolSize{DefaultPoolSize_};
    static constexpr std::optional<int32> MaxPoolSize{
        MaxPoolSize_ < 1 ? std::nullopt : std::optional<int32>{MaxPoolSize_}};

    static TSubclassOf<ActorType> GetDefaultClass() { return ActorType::StaticClass(); }
};
