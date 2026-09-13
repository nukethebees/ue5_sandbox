#include "sandbox/simulation/fighter_firing.h"

#include "sandbox/core/vector_math.h"
#include "sandbox/simulation/fighter_firing_scratch.h"

#include <cassert>
#include <cstddef>

namespace ml::simulation::fighters {
void prepare_firing(FiringPreparationView const fighters,
                    FiringPreparationParameters const parameters,
                    FiringScratch& scratch) {
    auto const count{fighters.locations.num()};
    [[maybe_unused]] auto const size{static_cast<std::size_t>(count)};
    assert(fighters.aim_directions.num() == count);
    assert(fighters.desired_aiming_directions.num() == count);
    assert(fighters.target_locations.num() == count);
    assert(fighters.handles.size() == size && fighters.targets.size() == size);
    assert(fighters.target_distance_squared.size() == size);
    assert(fighters.target_radii.size() == size);
    assert(fighters.attack_cooldowns.size() == size);
    assert(parameters.retry_cooldown >= 0);

    auto& aiming_dot_products{scratch.aiming_dot_products};
    auto& can_fire{scratch.can_fire};
    auto& trace_starts{scratch.line_of_sight_starts};
    auto& trace_ends{scratch.line_of_sight_ends};
    auto& ignored_entities{scratch.ignored_entities};
    aiming_dot_products.set_num(count);
    ml::native_math::dot_product_vector(aiming_dot_products.data(),
                                        fighters.aim_directions.xs.data(),
                                        fighters.aim_directions.ys.data(),
                                        fighters.aim_directions.zs.data(),
                                        fighters.desired_aiming_directions.xs.data(),
                                        fighters.desired_aiming_directions.ys.data(),
                                        fighters.desired_aiming_directions.zs.data(),
                                        count);

    can_fire.clear();
    can_fire.reserve(count);
    for (std::int32_t index{}; index < count; ++index) {
        auto const element{static_cast<std::size_t>(index)};
        if (fighters.attack_cooldowns[element] > 0) {
            continue;
        }
        if (fighters.target_distance_squared[element] > parameters.maximum_distance_squared ||
            !fighters.targets[element].is_valid() ||
            aiming_dot_products[index] < parameters.aim_threshold) {
            fighters.attack_cooldowns[element] = parameters.retry_cooldown;
            continue;
        }
        can_fire.add(index);
    }

    auto const firing_count{can_fire.num()};
    trace_starts.set_num(firing_count);
    trace_ends.set_num(firing_count);
    ignored_entities.set_num(firing_count);
    for (std::int32_t index{}; index < firing_count; ++index) {
        auto const fighter_index{can_fire[index]};
        auto const element{static_cast<std::size_t>(fighter_index)};
        auto const direction{fighters.aim_directions[fighter_index]};
        auto const end_offset{parameters.line_of_sight_buffer + fighters.target_radii[element]};
        ignored_entities[index] = fighters.handles[element];
        trace_starts.set(
            index, fighters.locations[fighter_index] + direction * parameters.fire_point_distance);
        trace_ends.set(index, fighters.target_locations[fighter_index] - direction * end_offset);
    }
}

void resolve_firing_visibility(Vectors3fConstView const locations,
                               Vectors3fConstView const desired_move_locations,
                               std::span<std::int16_t> const attack_cooldowns,
                               std::span<std::uint8_t const> const visibility_results,
                               float const arrival_distance_squared,
                               std::int16_t const retry_cooldown,
                               FrameArray<std::int32_t>& can_fire,
                               FrameArray<std::int32_t>& fighters_to_reposition) {
    assert(locations.num() == desired_move_locations.num());
    assert(attack_cooldowns.size() == static_cast<std::size_t>(locations.num()));
    assert(visibility_results.size() == static_cast<std::size_t>(can_fire.num()));
    assert(retry_cooldown >= 0);

    auto const count{can_fire.num()};
    for (auto index{count - 1}; index >= 0; --index) {
        auto const fighter_index{can_fire[index]};
        if (visibility_results[static_cast<std::size_t>(index)] != 0) {
            continue;
        }

        can_fire.remove_at_swap(index);
        assert(fighter_index >= 0 && fighter_index < locations.num());
        attack_cooldowns[static_cast<std::size_t>(fighter_index)] = retry_cooldown;
        auto const offset{locations[fighter_index] - desired_move_locations[fighter_index]};
        auto const distance_squared{HMM_LenSqrV3(offset)};
        if (distance_squared <= arrival_distance_squared) {
            fighters_to_reposition.add(fighter_index);
        }
    }
}

void accept_visible_firing_positions(Vectors3fView const desired_move_locations,
                                     Vectors3fConstView const candidate_locations,
                                     std::span<std::uint8_t const> const visibility_results,
                                     FrameArray<std::int32_t>& fighters_to_reposition) {
    auto const count{fighters_to_reposition.num()};
    assert(candidate_locations.num() == count);
    assert(visibility_results.size() == static_cast<std::size_t>(count));

    for (auto index{count - 1}; index >= 0; --index) {
        if (visibility_results[static_cast<std::size_t>(index)] == 0) {
            continue;
        }

        auto const fighter_index{fighters_to_reposition[index]};
        assert(fighter_index >= 0 && fighter_index < desired_move_locations.num());
        desired_move_locations.set(fighter_index, candidate_locations[index]);
        fighters_to_reposition.remove_at_swap(index);
    }
}
}
