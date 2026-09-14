#pragma once

#include "ioj/sim/attack_distance_band.h"

#include <cstdint>

namespace ioj::sim {
struct LaserWeaponSimConfig {
    std::int32_t damage{5};
    float projectile_speed{10000.f};
    float max_distance{10000.f};
    float fire_cooldown{0.33f};
};

struct LaserSimConfig {
    std::int32_t n_preallocated_instances{5000};
    std::int32_t collision_jobs{8};
};

struct OverlapResponseConfig {
    std::int32_t damage_per_overlap_detection{50};
};

struct FighterSimConfig {
    std::int32_t max_live_fighters{2000};
    float fire_dot_product_threshold{0.95f};
    float speed{2000.f};
    float turn_speed_unitless{1.f};
    float avoidance_clear_update_frequency{2.f};
    float avoidance_update_frequency{5.f};
    float avoidance_active_update_frequency{12.f};
    float avoidance_immediate_update_frequency{30.f};
    float avoidance_lookahead_time{1.f};
    float avoidance_clearance_buffer{100.f};
    float separation_radius{2000.f};
    float separation_strength{1.f};
    float steering_memory_duration{0.75f};
    std::int32_t dense_traffic_neighbour_threshold{4};
    LaserWeaponSimConfig laser{};
    std::int32_t health{50};
    float attack_retry_cooldown{0.15f};
    float attack_engagement_threshold{5000.f};
    float attack_reposition_frequency{10.f};
    AttackDistanceBand attack_distance_band;
    float arrival_distance{500.f};
    float los_check_buffer{100.f};
    float awareness_radius{10000.f};
    float awareness_scan_frequency{6.f};
    float minimum_opportunistic_intercept_deviation_dot_product{0.5f};
};
}
