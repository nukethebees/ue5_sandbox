#include "ioj/sim/capital_fighter_reassignment.h"

#include "ioj/sim/capital_ship_queries.h"
#include "ioj/sim/entity_types.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>

namespace ioj::sim {
auto assign_spawned_fighters(std::span<RegistryEntityHandle const> const capital_handles,
                             std::span<std::byte const> const capital_teams,
                             SpawnedEntityHandles const& spawned_handles,
                             std::span<RegistryEntityHandle const> const spawn_parents,
                             std::span<std::byte const> const spawn_teams,
                             EntityRegistryQueryView const registry,
                             ioj::sim::capital_ships::FighterReassignment& reassignments,
                             ml::FrameArray<RegistryEntityHandle>& fighters_to_self_destruct)
    -> std::int32_t {
    assert(capital_handles.size() == capital_teams.size());
    assert(spawn_parents.size() == spawn_teams.size());

    auto const spawn_count{
        std::min(static_cast<std::size_t>(spawned_handles.num()), spawn_parents.size())};
    std::int32_t surviving_spawn_count{};
    for (std::size_t spawn_index{}; spawn_index < spawn_count; ++spawn_index) {
        auto const fighter{spawned_handles.get_handle(static_cast<std::int32_t>(spawn_index))};
        if (!is_valid_alive(registry, fighter)) {
            continue;
        }

        auto destination{spawn_parents[spawn_index]};
        if (!find_capital_ship_index(capital_handles, destination)) {
            auto const team{
                static_cast<Team>(std::to_integer<std::uint8_t>(spawn_teams[spawn_index]))};
            auto const replacement{find_first_capital_ship_on_team(capital_teams, team)};
            if (!replacement) {
                fighters_to_self_destruct.add(fighter);
                continue;
            }
            destination = capital_handles[static_cast<std::size_t>(*replacement)];
        }

        reassignments.add(destination, fighter);
        ++surviving_spawn_count;
    }

    return surviving_spawn_count;
}

auto rebuild_fighter_rosters(std::span<RegistryEntityHandle const> const capital_handles,
                             std::span<IndexSpan> const fighter_spans,
                             std::span<RegistryEntityHandle const> const previous_fighters,
                             ioj::sim::capital_ships::FighterReassignment& reassignments,
                             std::span<RegistryEntityHandle> const output_fighters)
    -> std::int32_t {
    assert(fighter_spans.size() == capital_handles.size());
    assert(output_fighters.size() >=
           previous_fighters.size() + static_cast<std::size_t>(reassignments.num()));
    std::int32_t output_count{};
    auto const capital_count{static_cast<std::int32_t>(capital_handles.size())};
    for (std::int32_t capital_index{}; capital_index < capital_count; ++capital_index) {
        auto const capital_element{static_cast<std::size_t>(capital_index)};
        auto const old_span{fighter_spans[capital_element]};
        auto const old_end{old_span.end()};
        assert(old_span.offset >= 0 && old_span.count >= 0);
        assert(static_cast<std::size_t>(old_end) <= previous_fighters.size());
        IndexSpan new_span{.offset = output_count, .count = 0};
        for (auto fighter_index{old_span.offset}; fighter_index < old_end; ++fighter_index) {
            auto const fighter{previous_fighters[static_cast<std::size_t>(fighter_index)]};
            if (!fighter.is_null()) {
                output_fighters[static_cast<std::size_t>(output_count++)] = fighter;
            }
        }

        auto const reassigned_count{reassignments.num()};
        for (auto index{reassigned_count - 1}; index >= 0; --index) {
            auto const destination{reassignments.capital_handles[index]};
            auto const found{std::ranges::find(capital_handles, destination)};
            assert(found != capital_handles.end());
            if (found - capital_handles.begin() == capital_index) {
                output_fighters[static_cast<std::size_t>(output_count++)] =
                    reassignments.fighter_handles[index];
                reassignments.remove_at_swap(index, 1);
            }
        }
        new_span.count = output_count - new_span.offset;
        fighter_spans[capital_element] = new_span;
    }
    return output_count;
}

void plan_fighter_reassignment(std::span<RegistryEntityHandle const> const capital_handles,
                               std::span<std::byte const> const capital_teams,
                               std::span<IndexSpan const> const fighter_spans,
                               std::span<RegistryEntityHandle const> const fighter_handles,
                               std::span<std::int32_t const> const dying_capital_indices,
                               ioj::sim::capital_ships::FighterReassignment& reassignments,
                               ml::FrameArray<RegistryEntityHandle>& fighters_to_self_destruct) {
    assert(capital_teams.size() == capital_handles.size());
    assert(fighter_spans.size() == capital_handles.size());

    constexpr auto team_count{static_cast<std::size_t>(Team::COUNT)};
    std::array<std::int32_t, team_count> replacements{};
    replacements.fill(-1);
    std::array<bool, team_count> needs_replacement{};

    for (auto const capital_index : dying_capital_indices) {
        assert(capital_index >= 0);
        auto const capital_element{static_cast<std::size_t>(capital_index)};
        assert(capital_element < capital_handles.size());
        auto const team{std::to_integer<std::uint8_t>(capital_teams[capital_element])};
        assert(static_cast<std::size_t>(team) < team_count);
        needs_replacement[team] = true;
    }

    auto teams_remaining{static_cast<std::int32_t>(std::ranges::count(needs_replacement, true))};
    auto const capital_count{static_cast<std::int32_t>(capital_handles.size())};
    for (std::int32_t capital_index{}; capital_index < capital_count && teams_remaining > 0;
         ++capital_index) {
        auto const capital_element{static_cast<std::size_t>(capital_index)};
        auto const team{std::to_integer<std::uint8_t>(capital_teams[capital_element])};
        if (!needs_replacement[team] || std::ranges::find(dying_capital_indices, capital_index) !=
                                            dying_capital_indices.end()) {
            continue;
        }

        replacements[team] = capital_index;
        needs_replacement[team] = false;
        --teams_remaining;
    }

    for (auto const capital_index : dying_capital_indices) {
        auto const capital_element{static_cast<std::size_t>(capital_index)};
        auto const team{std::to_integer<std::uint8_t>(capital_teams[capital_element])};
        auto const replacement_index{replacements[team]};
        auto const fighter_span{fighter_spans[capital_element]};
        assert(fighter_span.offset >= 0 && fighter_span.count >= 0);
        assert(static_cast<std::size_t>(fighter_span.end()) <= fighter_handles.size());

        for (auto fighter_index{fighter_span.offset}; fighter_index < fighter_span.end();
             ++fighter_index) {
            auto const fighter{fighter_handles[static_cast<std::size_t>(fighter_index)]};
            if (replacement_index < 0) {
                fighters_to_self_destruct.add(fighter);
            } else {
                reassignments.add(capital_handles[static_cast<std::size_t>(replacement_index)],
                                  fighter);
            }
        }
    }
}
} // namespace ioj::sim
