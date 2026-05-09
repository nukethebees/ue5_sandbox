#include "FloatBounds.h"

auto FFloatBounds::get_rand() const -> float {
    return FMath::FRandRange(min, max);
}
