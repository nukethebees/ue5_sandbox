#pragma once

#include "Math/UnrealMathUtility.h"

namespace ml {
auto normalised_percent(float value, float max) {
    return value / max;
}
auto denormalise_percent_to_int(float pc) {
    return FMath::RoundToInt(pc * 100.0f);
}
auto clamp_normalised_percent(float pc) {
    return FMath::Clamp(pc, 0.0f, 1.0f);
}
}
