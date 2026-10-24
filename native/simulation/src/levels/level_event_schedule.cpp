#include <ioj/sim/levels/level_event_schedule.h>

#include <ioj/diagnostics.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <format>
#include <optional>
#include <span>
#include <vector>

namespace ioj::sim {
/* **************************************** */
// Participating teams
/* **************************************** */
auto LevelEventSchedule::get_participating_teams(std::optional<Team> const player_team) const
    -> TeamList {
    std::array<std::uint8_t, enum_count<Team>()> included{};
    auto const included_count{included.size()};
    auto include = [&included](Team const team) {
        auto const team_index{static_cast<std::int32_t>(team)};
        if (team_index >= 0 && static_cast<std::size_t>(team_index) < included_count) {
            included[team_index] = 1;
        } else {
            log_error(std::format("Ignoring invalid participating team {}", team_index));
        }
    };

    if (player_team.has_value()) {
        include(*player_team);
    }
    for (auto const team : capital_spawns.get_const_view().teams()) {
        include(team);
    }
    for (auto const team : turret_spawns.get_const_view().teams()) {
        include(team);
    }

    TeamList result;
    for (std::int32_t team_index{}; static_cast<std::size_t>(team_index) < included_count;
         ++team_index) {
        if (included[team_index] != 0) {
            result.add(static_cast<Team>(team_index));
        }
    }

    return result;
}

void LevelEventSchedule::add_initial_spawn_group(EntityType const type,
                                                 std::int32_t offset,
                                                 std::int32_t count) {
    assert(offset >= 0 && count >= 0);
    assert(spawn_groups.num() >= initial_spawn_group_count);

    // Split initial batches to fit the group's payload count without limiting the whole batch.
    // Rebind columns each iteration because appending a group can reallocate their storage.
    // NOLINTBEGIN(ioj-loop-view-accessor-call)
    while (count > 0) {
        if (initial_spawn_group_count > 0) {
            auto const groups{spawn_groups.get_view()};
            auto const last{initial_spawn_group_count - 1};
            auto const previous_count{groups.counts()[last]};
            if (groups.types()[last] == type && groups.offsets()[last] + previous_count == offset &&
                previous_count < max_level_event_count) {
                auto const added{std::min(
                    count, static_cast<std::int32_t>(max_level_event_count - previous_count))};
                groups.counts()[last] = static_cast<LevelEventCount>(previous_count + added);
                offset += added;
                count -= added;
                continue;
            }
        }

        auto const added{std::min(count, static_cast<std::int32_t>(max_level_event_count))};
        auto const old_count{spawn_groups.num()};
        spawn_groups.add_uninitialised(1);
        auto const groups{spawn_groups.get_view()};
        auto const types{groups.types()};
        auto const offsets{groups.offsets()};
        auto const counts{groups.counts()};

        // Keep scheduled groups after the initial prefix when extending a compiled level.
        for (auto row{old_count}; row > initial_spawn_group_count; --row) {
            types[row] = types[row - 1];
            offsets[row] = offsets[row - 1];
            counts[row] = counts[row - 1];
        }

        auto const index{initial_spawn_group_count};
        types[index] = type;
        offsets[index] = offset;
        counts[index] = static_cast<LevelEventCount>(added);
        ++initial_spawn_group_count;
        offset += added;
        count -= added;
    }
    // NOLINTEND(ioj-loop-view-accessor-call)
}

auto LevelEventSchedule::add_spawn_group(EntityType const type,
                                         std::int32_t const offset,
                                         std::int32_t const count) -> bool {
    if (count == 0) {
        return true;
    }
    assert(!event_group_counts.empty());
    assert(execution_ticks.size() == event_group_counts.size());

    LevelEventCount payload_count;
    auto& tick_counts{event_group_counts.back()};
    LevelEventCount group_count;
    if (!try_to_level_event_count(count, payload_count) ||
        !try_to_level_event_count(tick_counts.spawn_groups + 1, group_count)) {
        return false;
    }

    tick_counts.spawn_groups = group_count;
    auto const index{spawn_groups.num()};
    spawn_groups.add_uninitialised(1);
    auto const groups{spawn_groups.get_view()};
    groups.types()[index] = type;
    groups.offsets()[index] = offset;
    groups.counts()[index] = payload_count;
    return true;
}

}
