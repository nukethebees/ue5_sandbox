#pragma once

#include "Engine/EngineTypes.h"

namespace ml::collision {
static constexpr ECollisionChannel projectile{ECC_GameTraceChannel1};
static constexpr ECollisionChannel interaction{ECC_GameTraceChannel2};
}
