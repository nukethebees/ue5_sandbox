#pragma once

#include <ioj/sim/lasers/laser_weapon_sim_config.h>
#include <ioj/sim/spinners/fire_point.h>

#include <vector>

namespace ioj::sim {
struct SpinnerSimConfig {
    std::vector<spinners::FirePoint> fire_point_offsets;
    float yaw_rotation_speed_degrees{66.f};
    LaserWeaponSimConfig laser{};
};
}
