#pragma once

#include "CoreMinimal.h"

namespace ml::ioj {

enum class EGameWindowMode : uint8 {
    Windowed,
    Borderless,
    Fullscreen,
};

enum class EGameAntiAliasingMethod : uint8 {
    Off,
    FXAA,
    TAA,
    TSR,
    SMAA,
};

enum class EGameQualityLevel : uint8 {
    Low,
    Medium,
    High,
    Epic,
};

} // namespace ml::ioj
