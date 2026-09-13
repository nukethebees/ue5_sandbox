#pragma once

#include "sandbox/simulation/entity_handle.h"
#include "sandbox/simulation/vector_types.h"
#include "sandbox/simulation/vectors3f.h"

#include <array>
#include <cstdint>
#include <optional>
#include <span>

namespace ml::simulation::fighters {
inline constexpr std::int8_t avoidance_direction_count{8};

enum class NavigationRiskTier : std::uint8_t {
    Clear,
    Nearby,
    Active,
    Immediate,
    Count,
};

struct AvoidanceFrame {
    Vector3f preferred_direction;
    Vector3f first_lateral;
    Vector3f second_lateral;
    float roll_sin;
    float roll_cos;
};

struct NavigationRiskUpdate {
    NavigationRiskTier tier;
    std::uint8_t lower_risk_scan_count;
};

[[nodiscard]] constexpr auto is_avoidance_direction_choice(std::int8_t choice) noexcept -> bool {
    return choice >= 0 && choice < avoidance_direction_count;
}

[[nodiscard]] auto make_avoidance_frame(Vector3f preferred_direction, float float_bias) noexcept
    -> AvoidanceFrame;
[[nodiscard]] auto make_avoidance_direction(AvoidanceFrame const& frame,
                                            std::int8_t choice) noexcept -> Vector3f;
void
    make_avoidance_directions(AvoidanceFrame const& frame,
                              std::array<Vector3f, avoidance_direction_count>& directions) noexcept;
void make_avoidance_choice_order(
    std::uint32_t integral_bias,
    std::int8_t previous_choice,
    std::array<std::int8_t, avoidance_direction_count>& choice_order) noexcept;
[[nodiscard]] auto make_coincident_separation_direction(FRegistryEntityHandle self,
                                                        FRegistryEntityHandle other) noexcept
    -> Vector3f;
[[nodiscard]] auto classify_navigation_risk(float closest_distance_squared,
                                            float immediate_distance_squared,
                                            float close_distance_squared,
                                            std::int32_t nearby_count) noexcept
    -> NavigationRiskTier;
[[nodiscard]] auto update_navigation_risk(NavigationRiskTier current_tier,
                                          NavigationRiskTier observed_tier,
                                          std::uint8_t lower_risk_scan_count,
                                          std::uint8_t scans_to_demote) noexcept
    -> NavigationRiskUpdate;
[[nodiscard]] auto make_separation_steering_direction(Vector3f goal_direction,
                                                      Vector3f separation_steering,
                                                      float separation_strength) noexcept
    -> std::optional<Vector3f>;
[[nodiscard]] auto choose_navigation_alternative(Vector3f fighter_location,
                                                 float safe_progress_distance,
                                                 std::span<std::int8_t const> choices,
                                                 std::span<std::uint8_t const> in_world,
                                                 std::span<std::uint8_t const> hits,
                                                 Vectors3fConstView hit_locations,
                                                 std::int8_t stop_choice) noexcept -> std::int8_t;
} // namespace ml::simulation::fighters
