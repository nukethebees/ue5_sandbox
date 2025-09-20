#pragma once

#include "Math/UnrealMathUtility.h"

namespace ml {
inline auto normalised_percent(float value, float max) {
    return value / max;
}
inline auto denormalise_percent_to_int(float pc) {
    return FMath::RoundToInt(pc * 100.0f);
}
inline auto clamp_normalised_percent(float pc) {
    return FMath::Clamp(pc, 0.0f, 1.0f);
}

// Calculate explosion damage based on linear distance falloff
inline float calculate_explosion_damage(float distance, float explosion_radius, float max_damage) {
    if (distance >= explosion_radius) {
        return 0.0f;
    }

    auto const falloff_factor{1.0f - (distance / explosion_radius)};
    return max_damage * falloff_factor;
}
}
