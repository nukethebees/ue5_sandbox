#pragma once

#include <ioj/math_types.h>
#include <ioj/sim/entities/health_type.h>
#include <ioj/sim/lasers/laser_weapon_sim_config.h>

namespace ioj::sim {
struct TurretSimConfig {
    float detection_radius{3000.f};
    float target_refresh_frequency{5.f};
    Vector3f fire_point_offset{};
    LaserWeaponSimConfig laser{};
    Health max_health{20};
};
}
