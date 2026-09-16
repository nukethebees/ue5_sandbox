#pragma once

#include "ioj/sim/entity_unique_id.h"
#include "ioj/sim/vector_types.h"
#include "ioj/sim/vectors3f.h"

#include <array>
#include <cstdint>
#include <optional>
#include <span>

namespace ioj::sim::fighters {
inline constexpr std::int8_t avoidance_direction_count{8};
inline constexpr std::int32_t separation_neighbour_limit{32};

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

struct SeparationObservationParameters {
    float separation_radius;
    float immediate_distance_squared;
    float close_distance_squared;
    float memory_retention;
    float separation_strength;
    std::int32_t dense_traffic_neighbour_threshold;
    std::uint32_t integral_bias;
    float float_bias;
};

struct SeparationObservation {
    Vector3f steering_memory;
    NavigationRiskTier risk_tier;
    bool dense_direction_selected;
};

struct SeparationNeighbour {
    EntityUniqueId id;
    Vector3f location;
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
[[nodiscard]] auto make_coincident_separation_direction(EntityUniqueId self,
                                                        EntityUniqueId other) noexcept -> Vector3f;
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
[[nodiscard]] auto observe_separation(Vector3f fighter_location,
                                      EntityUniqueId fighter_id,
                                      Vector3f goal_direction,
                                      Vector3f previous_memory,
                                      std::span<SeparationNeighbour const> neighbours,
                                      SeparationObservationParameters parameters) noexcept
    -> SeparationObservation;
[[nodiscard]] auto choose_navigation_alternative(Vector3f fighter_location,
                                                 float safe_progress_distance,
                                                 std::span<std::int8_t const> choices,
                                                 std::span<std::uint8_t const> in_world,
                                                 std::span<std::uint8_t const> hits,
                                                 Vectors3fConstView hit_locations,
                                                 std::int8_t stop_choice) noexcept -> std::int8_t;
} // namespace ioj::sim::fighters
