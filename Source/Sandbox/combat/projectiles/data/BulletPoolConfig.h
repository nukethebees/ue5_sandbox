#pragma once

#include <optional>

#include "CoreMinimal.h"

#include "Sandbox/combat/projectiles/actors/BulletActor.h"

struct FBulletPoolConfig {
    using ActorType = ABulletActor;
    static constexpr int32 DefaultPoolSize{100};
    static constexpr std::optional<int32> MaxPoolSize{std::nullopt};

    static TSubclassOf<ABulletActor> GetDefaultClass();
};
