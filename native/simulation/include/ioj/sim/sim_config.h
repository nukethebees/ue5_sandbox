#pragma once
#include <ioj/sim/base_sim_config.h>
#include <ioj/sim/transform3d.h>

#include <cstdint>
#include <vector>

namespace ioj::sim {

namespace spinners {
struct FirePoint {
    Vector3f location;
    Rotator3f rotation;
};
}

struct PlayerSimConfig {
    float thrust_energy_max{1.f};
    float pitch_angle_max{30.f};
    float pitch_speed{3.f};
    float yaw_angle_max{30.f};
    float yaw_speed{3.f};
    float turn_bank_angle_max{30.f};
    float turn_bank_speed{2.f};
    LaserWeaponSimConfig laser{};
    float laser_lock_on_transition_delay{1.f};
    float laser_lock_on_distance{10000.f};
};

struct TurretSimConfig {
    std::int32_t search_slice_size{64};
    float detection_radius{3000.f};
    float target_refresh_frequency{5.f};
    Vector3f fire_point_offset{};
    LaserWeaponSimConfig laser{};
    Health max_health{20};
};

struct SpinnerSimConfig {
    std::vector<spinners::FirePoint> fire_point_offsets;
    float yaw_rotation_speed_degrees{66.f};
    LaserWeaponSimConfig laser{};
};

struct CapitalShipSimConfig {
    float spawn_delay{5.f};
    std::int32_t fighter_spawn_slots{};
    std::vector<Transform3d> fighter_spawn_slots_relative_transforms;
    Health max_health{5000};
};
} // namespace ioj::sim
