#include "Sandbox/players/VisionConfig.h"

auto FVisionConfig::half_angle_degrees() const -> float {
    return half_angle_deg;
}
auto FVisionConfig::half_angle_rad() const -> float {
    return FMath::DegreesToRadians(half_angle_deg);
}
