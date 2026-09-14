#include "ioj/sim/fighter_navigation_scans.h"

#include "ioj/sim/fighter_navigation.h"
#include "ioj/sim/fighter_navigation_scratch.h"
#include "sandbox/core/vector_math.h"

#include <array>
#include <cassert>
#include <cstddef>

namespace ioj::sim::fighters {
namespace navigation_scans_detail {
auto lookahead_distance(float const speed,
                        float const time,
                        float const minimum,
                        float const move_distance) noexcept -> float {
    auto const speed_distance{speed * time};
    auto const requested_distance{speed_distance >= minimum ? speed_distance : minimum};
    return move_distance <= requested_distance ? move_distance : requested_distance;
}
}

void prepare_preferred_navigation(NavigationScanView const fighters,
                                  float const lookahead_time,
                                  float const minimum_lookahead_distance,
                                  float const direction_zero_tolerance,
                                  NavigationScratch& scratch) {
    auto const count{scratch.ready_fighter_indices.num()};
    scratch.line_of_sight_starts.reserve(count);
    scratch.line_of_sight_ends.reserve(count);
    scratch.trace_fighter_indices.reserve(count);
    scratch.blocked_fighter_indices.reserve(count);

    for (auto const index : scratch.ready_fighter_indices) {
        auto const element{static_cast<std::size_t>(index)};
        auto const direction{fighters.preferred_directions[index]};
        auto const move_distance{fighters.move_distances[element]};
        if (ml::native_math::is_nearly_zero(
                direction.X, direction.Y, direction.Z, direction_zero_tolerance) ||
            move_distance <= 0.f) {
            continue;
        }

        auto const distance{navigation_scans_detail::lookahead_distance(
            fighters.speeds[element], lookahead_time, minimum_lookahead_distance, move_distance)};
        auto const start{fighters.locations[index]};
        scratch.line_of_sight_starts.add(start);
        scratch.line_of_sight_ends.add(start + direction * distance);
        scratch.trace_fighter_indices.add(index);
    }
}

void resolve_preferred_navigation(NavigationScratch& scratch,
                                  std::span<std::int8_t> const choices,
                                  std::span<std::uint8_t> const clear_scan_counts,
                                  std::int8_t const direct_choice,
                                  std::uint8_t const clear_scans_to_end_avoidance) {
    auto const count{scratch.trace_fighter_indices.num()};
    assert(scratch.line_of_sight_results.num() == count);
    assert(scratch.trace_hits.num() == count);
    assert(choices.size() == clear_scan_counts.size());

    for (std::int32_t index{}; index < count; ++index) {
        auto const fighter_index{scratch.trace_fighter_indices[index]};
        auto const element{static_cast<std::size_t>(fighter_index)};
        assert(fighter_index >= 0 && element < choices.size());
        if (scratch.line_of_sight_results[index] == 0 || scratch.trace_hits.hits[index] != 0) {
            scratch.blocked_fighter_indices.add(fighter_index);
            clear_scan_counts[element] = 0;
            continue;
        }
        if (choices[element] == direct_choice) {
            clear_scan_counts[element] = 0;
            continue;
        }

        // One clear scan at an obstacle edge must not discard a held avoidance choice.
        auto& clear_count{clear_scan_counts[element]};
        ++clear_count;
        if (clear_count >= clear_scans_to_end_avoidance) {
            choices[element] = direct_choice;
            clear_count = 0;
        }
    }
}

void prepare_alternative_navigation(NavigationScanView const fighters,
                                    float const lookahead_time,
                                    float const minimum_lookahead_distance,
                                    NavigationScratch& scratch) {
    // Direct results have been consumed; reuse their storage for alternative traces.
    scratch.trace_fighter_indices.clear();
    scratch.trace_choice_indices.clear();
    scratch.line_of_sight_starts.clear();
    scratch.line_of_sight_ends.clear();
    scratch.line_of_sight_results.clear();
    scratch.trace_hits.clear();

    auto const count{scratch.blocked_fighter_indices.num() * avoidance_direction_count};
    scratch.line_of_sight_starts.reserve(count);
    scratch.line_of_sight_ends.reserve(count);
    scratch.trace_choice_indices.reserve(count);

    for (auto const index : scratch.blocked_fighter_indices) {
        auto const element{static_cast<std::size_t>(index)};
        auto const lookahead_distance{
            navigation_scans_detail::lookahead_distance(fighters.speeds[element],
                                                        lookahead_time,
                                                        minimum_lookahead_distance,
                                                        fighters.move_distances[element])};
        auto const start{fighters.locations[index]};
        auto const frame{make_avoidance_frame(fighters.preferred_directions[index],
                                              fighters.float_biases[element])};
        std::array<Vector3f, avoidance_direction_count> directions;
        make_avoidance_directions(frame, directions);
        std::array<std::int8_t, avoidance_direction_count> order;
        make_avoidance_choice_order(
            fighters.integral_biases[element], fighters.choices[element], order);

        for (auto const choice : order) {
            auto const direction{directions[static_cast<std::size_t>(choice)]};
            scratch.line_of_sight_starts.add(start);
            scratch.line_of_sight_ends.add(start + direction * lookahead_distance);
            scratch.trace_choice_indices.add(choice);
        }
    }
    assert(scratch.line_of_sight_ends.num() == count);
}
}
