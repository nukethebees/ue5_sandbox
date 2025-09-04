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
}
