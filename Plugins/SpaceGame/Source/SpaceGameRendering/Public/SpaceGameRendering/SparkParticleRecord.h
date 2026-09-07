#pragma once

#include <CoreMinimal.h>

struct alignas(16) SPACEGAMERENDERING_API FSparkParticleRecord {
    FVector4f initial_position_spawn_time{FVector4f::Zero()};
    FVector4f initial_velocity_lifetime{FVector4f::Zero()};
    FVector4f emissive_colour_size{FVector4f::Zero()};
    FVector4f streak_time_reserved{FVector4f::Zero()};
};

static_assert(sizeof(FSparkParticleRecord) == 64);
