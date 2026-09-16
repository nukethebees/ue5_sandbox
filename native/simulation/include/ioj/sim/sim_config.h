#pragma once
#include <ioj/sim/transform3d.h>

#include <cstdint>
#include <vector>

#include <ioj/sim/base_sim_config.h>
#include <sandbox/core/speed_response.h>

namespace ioj::sim {

namespace spinners {
struct FirePoint {
    Vector3f location;
    Rotator3f rotation;
};
}

struct PlayerSimConfig {
    float thrust_energy_max{1.f};
    ml::SpeedResponses speed_responses{};
    float cruise_speed{12000.f};
    float thrust_recharge_time{7.f};
    float boost_depletion_time{4.f};
    float boost_speed{30000.f};
    float boost_forward_speed_addition_multiplier{2.f};
    float brake_depletion_time{6.f};
    float brake_speed{1000.f};
    float rotation_speed{60.f};
    float pitch_angle_max{30.f};
    float pitch_speed{3.f};
    float yaw_angle_max{30.f};
    float yaw_speed{3.f};
    float turn_bank_angle_max{30.f};
    float turn_bank_speed{2.f};
    float manual_bank_angle_max{90.f};
    float manual_bank_speed{5.f};
    float auto_level_speed{10.f};
    float auto_level_roll_delay{1.f};
    float lateral_adjustment_speed{5000.f};
    float vertical_adjustment_speed{5000.f};
    float planar_lateral_trim_speed{3000.f};
    float planar_vertical_trim_speed{3000.f};
    float forward_velocity_trim_fraction{0.05f};
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
