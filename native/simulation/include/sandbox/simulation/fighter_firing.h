#pragma once

#include "sandbox/core/frame_array.h"
#include "sandbox/simulation/entity_handle.h"
#include "sandbox/simulation/frame_vectors3f.h"

#include <cstdint>
#include <span>

namespace ml::simulation::fighters {
struct FiringScratch;

struct FiringPreparationView {
    Vectors3fConstView locations;
    Vectors3fConstView aim_directions;
    Vectors3fConstView desired_aiming_directions;
    Vectors3fConstView target_locations;
    std::span<FRegistryEntityHandle const> handles;
    std::span<FRegistryEntityHandle const> targets;
    std::span<float const> target_distance_squared;
    std::span<float const> target_radii;
    std::span<std::int16_t> attack_cooldowns;
};

struct FiringPreparationParameters {
    float maximum_distance_squared;
    float aim_threshold;
    float fire_point_distance;
    float line_of_sight_buffer;
    std::int16_t retry_cooldown;
};

void prepare_firing(FiringPreparationView fighters,
                    FiringPreparationParameters parameters,
                    FiringScratch& scratch);

void resolve_firing_visibility(Vectors3fConstView locations,
                               Vectors3fConstView desired_move_locations,
                               std::span<std::int16_t> attack_cooldowns,
                               std::span<std::uint8_t const> visibility_results,
                               float arrival_distance_squared,
                               std::int16_t retry_cooldown,
                               FrameArray<std::int32_t>& can_fire,
                               FrameArray<std::int32_t>& fighters_to_reposition);

void accept_visible_firing_positions(Vectors3fView desired_move_locations,
                                     Vectors3fConstView candidate_locations,
                                     std::span<std::uint8_t const> visibility_results,
                                     FrameArray<std::int32_t>& fighters_to_reposition);
}
