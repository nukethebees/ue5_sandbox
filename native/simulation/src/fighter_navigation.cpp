#include "sandbox/simulation/fighter_navigation.h"

#include "sandbox/simulation/deterministic_bias.h"

#include <algorithm>
#include <bit>
#include <cassert>
#include <cmath>

namespace ml::simulation::fighters {
namespace {
inline constexpr float half_weight{0.5f};
inline constexpr float sqrt_three_over_two{0.8660254f};
inline constexpr float escape_forward_weight{-0.1736482f};
inline constexpr float escape_lateral_weight{0.9848078f};
inline constexpr float pi{3.14159265358979323846f};
inline constexpr float two_pi{6.28318530717958647692f};
inline constexpr float half_pi{1.57079632679489661923f};
inline constexpr float inverse_pi{0.31830988618379067154f};
inline constexpr float safe_normal_tolerance{1.e-8f};
inline constexpr float nearly_zero_tolerance{1.e-4f};

auto is_nearly_zero(Vector3f const vector) noexcept -> bool {
    return std::abs(vector.X) <= nearly_zero_tolerance &&
           std::abs(vector.Y) <= nearly_zero_tolerance &&
           std::abs(vector.Z) <= nearly_zero_tolerance;
}

auto safe_normal(Vector3f const vector) noexcept -> Vector3f {
    auto const length_squared{HMM_DotV3(vector, vector)};
    if (length_squared == 1.0f) {
        return vector;
    }
    if (length_squared < safe_normal_tolerance) {
        return make_vector3f(0.0f, 0.0f, 0.0f);
    }
    return vector * (1.0f / std::sqrt(length_squared));
}

constexpr auto hash_entity_handle(FRegistryEntityHandle const handle) noexcept -> std::uint32_t {
    auto const value{std::bit_cast<std::uint64_t>(handle)};
    return static_cast<std::uint32_t>(value) + static_cast<std::uint32_t>(value >> 32) * 23u;
}

constexpr auto combine_hashes(std::uint32_t const first, std::uint32_t const second) noexcept
    -> std::uint32_t {
    return first ^ (second + 0x9e3779b9u + (first << 6) + (first >> 2));
}

void sin_cos(float const value, float& sine, float& cosine) noexcept {
    // Match FMath::SinCos so moving this calculation across the library boundary is neutral.
    auto quotient{inverse_pi * 0.5f * value};
    quotient = value >= 0.0f ? static_cast<float>(static_cast<std::int64_t>(quotient + 0.5f))
                             : static_cast<float>(static_cast<std::int64_t>(quotient - 0.5f));
    auto angle{value - two_pi * quotient};

    float cosine_sign;
    if (angle > half_pi) {
        angle = pi - angle;
        cosine_sign = -1.0f;
    } else if (angle < -half_pi) {
        angle = -pi - angle;
        cosine_sign = -1.0f;
    } else {
        cosine_sign = 1.0f;
    }

    auto const angle_squared{angle * angle};
    sine =
        (((((-2.3889859e-08f * angle_squared + 2.7525562e-06f) * angle_squared - 0.00019840874f) *
               angle_squared +
           0.0083333310f) *
              angle_squared -
          0.16666667f) *
             angle_squared +
         1.0f) *
        angle;
    auto const cosine_polynomial{
        ((((-2.6051615e-07f * angle_squared + 2.4760495e-05f) * angle_squared - 0.0013888378f) *
              angle_squared +
          0.041666638f) *
             angle_squared -
         0.5f) *
            angle_squared +
        1.0f};
    cosine = cosine_sign * cosine_polynomial;
}
} // namespace

auto make_avoidance_frame(Vector3f const preferred_direction, float const float_bias) noexcept
    -> AvoidanceFrame {
    auto const reference_axis{std::abs(preferred_direction.Z) < 0.9f
                                  ? make_vector3f(0.0f, 0.0f, 1.0f)
                                  : make_vector3f(0.0f, 1.0f, 0.0f)};
    auto const first_lateral{safe_normal(HMM_Cross(reference_axis, preferred_direction))};
    auto const second_lateral{safe_normal(HMM_Cross(preferred_direction, first_lateral))};
    auto const roll_angle{float_bias * two_pi};
    float roll_sin;
    float roll_cos;
    sin_cos(roll_angle, roll_sin, roll_cos);

    return {
        preferred_direction,
        first_lateral,
        second_lateral,
        roll_sin,
        roll_cos,
    };
}

auto make_avoidance_direction(AvoidanceFrame const& frame, std::int8_t const choice) noexcept
    -> Vector3f {
    auto const ring{choice / 4};
    auto const ring_index{choice % 4};
    Vector3f lateral_direction;
    switch (ring_index) {
        case 0:
            lateral_direction =
                frame.first_lateral * frame.roll_cos + frame.second_lateral * frame.roll_sin;
            break;
        case 1:
            lateral_direction =
                frame.first_lateral * -frame.roll_sin + frame.second_lateral * frame.roll_cos;
            break;
        case 2:
            lateral_direction =
                frame.first_lateral * -frame.roll_cos + frame.second_lateral * -frame.roll_sin;
            break;
        default:
            lateral_direction =
                frame.first_lateral * frame.roll_sin + frame.second_lateral * -frame.roll_cos;
            break;
    }

    auto const forward_weight{ring == 0 ? sqrt_three_over_two : escape_forward_weight};
    auto const lateral_weight{ring == 0 ? half_weight : escape_lateral_weight};
    return frame.preferred_direction * forward_weight + lateral_direction * lateral_weight;
}

void make_avoidance_directions(
    AvoidanceFrame const& frame,
    std::array<Vector3f, avoidance_direction_count>& directions) noexcept {
    std::array<Vector3f, 4> const lateral_directions{
        frame.first_lateral * frame.roll_cos + frame.second_lateral * frame.roll_sin,
        frame.first_lateral * -frame.roll_sin + frame.second_lateral * frame.roll_cos,
        frame.first_lateral * -frame.roll_cos + frame.second_lateral * -frame.roll_sin,
        frame.first_lateral * frame.roll_sin + frame.second_lateral * -frame.roll_cos,
    };
    auto const shallow_forward{frame.preferred_direction * sqrt_three_over_two};
    auto const escape_forward{frame.preferred_direction * escape_forward_weight};

    for (std::int8_t ring_index{}; ring_index < 4; ++ring_index) {
        directions[ring_index] = shallow_forward + lateral_directions[ring_index] * half_weight;
        directions[ring_index + 4] =
            escape_forward + lateral_directions[ring_index] * escape_lateral_weight;
    }
}

void make_avoidance_choice_order(
    std::uint32_t const integral_bias,
    std::int8_t const previous_choice,
    std::array<std::int8_t, avoidance_direction_count>& choice_order) noexcept {
    std::int8_t write_index{};
    if (is_avoidance_direction_choice(previous_choice)) {
        choice_order[write_index++] = previous_choice;
    }

    auto const first_ring_index{static_cast<std::int8_t>(integral_bias % 4)};
    auto const direction{static_cast<std::int8_t>((integral_bias & 4u) == 0 ? 1 : -1)};
    for (std::int8_t ring{}; ring < 2; ++ring) {
        for (std::int8_t offset{}; offset < 4; ++offset) {
            auto const ring_index{
                static_cast<std::int8_t>((first_ring_index + direction * offset + 4) % 4)};
            auto const choice{static_cast<std::int8_t>(ring * 4 + ring_index)};
            if (choice != previous_choice) {
                choice_order[write_index++] = choice;
            }
        }
    }
}

auto make_coincident_separation_direction(FRegistryEntityHandle const self,
                                          FRegistryEntityHandle const other) noexcept -> Vector3f {
    auto const first{self < other ? self : other};
    auto const second{self < other ? other : self};
    auto const pair_hash{combine_hashes(hash_entity_handle(first), hash_entity_handle(second))};
    auto const biases{ml::make_deterministic_biases(
        static_cast<std::int32_t>(pair_hash), static_cast<std::int32_t>(pair_hash ^ 0x9e3779b9u))};
    auto const z{biases.floating * 2.0f - 1.0f};
    auto const radial{std::sqrt(std::max(0.0f, 1.0f - z * z))};
    auto const angle{static_cast<float>(biases.integral & 0x00ffffffu) * (two_pi / 16'777'216.0f)};
    float angle_sin;
    float angle_cos;
    sin_cos(angle, angle_sin, angle_cos);
    auto const direction{make_vector3f(radial * angle_cos, radial * angle_sin, z)};
    return self == first ? direction : -direction;
}

auto classify_navigation_risk(float const closest_distance_squared,
                              float const immediate_distance_squared,
                              float const close_distance_squared,
                              std::int32_t const nearby_count) noexcept -> NavigationRiskTier {
    if (closest_distance_squared <= immediate_distance_squared) {
        return NavigationRiskTier::Immediate;
    }
    if (closest_distance_squared <= close_distance_squared) {
        return NavigationRiskTier::Active;
    }
    if (nearby_count > 0) {
        return NavigationRiskTier::Nearby;
    }
    return NavigationRiskTier::Clear;
}

auto update_navigation_risk(NavigationRiskTier const current_tier,
                            NavigationRiskTier const observed_tier,
                            std::uint8_t lower_risk_scan_count,
                            std::uint8_t const scans_to_demote) noexcept -> NavigationRiskUpdate {
    if (observed_tier >= current_tier) {
        return {observed_tier, 0};
    }

    ++lower_risk_scan_count;
    if (lower_risk_scan_count >= scans_to_demote) {
        return {observed_tier, 0};
    }
    return {current_tier, lower_risk_scan_count};
}

auto make_separation_steering_direction(Vector3f const goal_direction,
                                        Vector3f const separation_steering,
                                        float const separation_strength) noexcept
    -> std::optional<Vector3f> {
    if (is_nearly_zero(separation_steering)) {
        return std::nullopt;
    }

    auto preferred_direction{
        safe_normal(goal_direction + separation_steering * separation_strength)};
    if (is_nearly_zero(preferred_direction)) {
        preferred_direction = safe_normal(separation_steering);
    }
    return preferred_direction;
}

auto choose_navigation_alternative(Vector3f const fighter_location,
                                   float const safe_progress_distance,
                                   std::span<std::int8_t const> const choices,
                                   std::span<std::uint8_t const> const in_world,
                                   std::span<std::uint8_t const> const hits,
                                   Vectors3fConstView const hit_locations,
                                   std::int8_t const stop_choice) noexcept -> std::int8_t {
    assert(choices.size() == in_world.size());
    assert(choices.size() == hits.size());
    assert(choices.size() == static_cast<std::size_t>(hit_locations.num()));

    auto chosen_choice{stop_choice};
    auto best_distance_squared{safe_progress_distance * safe_progress_distance};
    auto const count{static_cast<std::int32_t>(choices.size())};
    for (std::int32_t index{}; index < count; ++index) {
        auto const element{static_cast<std::size_t>(index)};
        if (in_world[element] == 0) {
            continue;
        }
        if (hits[element] == 0) {
            return choices[element];
        }

        auto const hit_location{hit_locations[index]};
        auto const dx{fighter_location.X - hit_location.X};
        auto const dy{fighter_location.Y - hit_location.Y};
        auto const dz{fighter_location.Z - hit_location.Z};
        auto const distance_squared{dx * dx + dy * dy + dz * dz};
        if (distance_squared > best_distance_squared) {
            best_distance_squared = distance_squared;
            chosen_choice = choices[element];
        }
    }
    return chosen_choice;
}
} // namespace ml::simulation::fighters
