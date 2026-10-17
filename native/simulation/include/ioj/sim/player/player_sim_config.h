#pragma once

#include <ioj/sim/lasers/laser_weapon_sim_config.h>

namespace ioj::sim {
struct PlayerSimConfig {
    float thrust_energy_max{1.f};
    float pitch_angle_max{30.f};
    float pitch_speed{3.f};
    float yaw_angle_max{30.f};
    float yaw_speed{3.f};
    float turn_bank_angle_max{30.f};
    float turn_bank_speed{2.f};
    LaserWeaponSimConfig laser{};
};
}
