#include "sandbox/simulation/turret_targeting.h"

#include "sandbox/core/loop_bounds.h"

#include <cassert>

namespace ml::simulation {
auto select_turret_target(std::span<FRegistryEntityHandle const> const candidates,
                          std::span<std::uint8_t const> const has_line_of_sight,
                          std::span<std::byte const> const registry_teams,
                          Team const turret_team,
                          std::uint32_t const integral_bias) noexcept -> FRegistryEntityHandle {
    assert(candidates.size() == has_line_of_sight.size());
    if (candidates.empty()) {
        return {};
    }

    auto const candidate_count{static_cast<std::int32_t>(candidates.size())};
    auto const target_offset{
        static_cast<std::int32_t>(integral_bias % static_cast<std::uint32_t>(candidate_count))};
    auto const loop_bounds{ml::make_rotated_loop_bounds(0, candidate_count, target_offset)};
    for (auto const bounds : loop_bounds) {
        for (auto candidate_index{bounds.begin}; candidate_index < bounds.end; ++candidate_index) {
            auto const candidate_element{static_cast<std::size_t>(candidate_index)};
            if (has_line_of_sight[candidate_element] == 0) {
                continue;
            }

            auto const candidate{candidates[candidate_element]};
            assert(candidate.index >= 0);
            auto const entity_element{static_cast<std::size_t>(candidate.index)};
            assert(entity_element < registry_teams.size());
            auto const candidate_team{
                std::to_integer<std::uint8_t>(registry_teams[entity_element])};
            if (candidate_team != static_cast<std::uint8_t>(turret_team)) {
                return candidate;
            }
        }
    }
    return {};
}
} // namespace ml::simulation
