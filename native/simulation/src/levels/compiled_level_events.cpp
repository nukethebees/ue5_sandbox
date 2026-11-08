#include <ioj/sim/levels/compiled_level_events.h>

#include <ioj/diagnostics.h>
#include <ioj/team_array.h>

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <format>
#include <limits>
#include <optional>
#include <span>
#include <vector>

namespace ioj::sim {
/* **************************************** */
// Participating teams
/* **************************************** */
auto CompiledLevelEvents::collect_participating_teams(std::optional<Team> const player_team) const
    -> TeamList {
    TeamArray<std::uint8_t> included{};
    auto const included_count{included.size()};
    auto include = [&included](Team const team) {
        auto const team_index{static_cast<std::int32_t>(team)};
        if (team_index >= 0 && static_cast<std::size_t>(team_index) < included_count) {
            included[team] = 1;
        } else {
            log_error(std::format("Ignoring invalid participating team {}", team_index));
        }
    };

    if (player_team.has_value()) {
        include(*player_team);
    }
    for (auto const team : capital_spawns.const_view().teams()) {
        include(team);
    }
    for (auto const team : turret_spawns.const_view().teams()) {
        include(team);
    }

    TeamList result;
    for (auto const team : EnumTraits<Team>::values) {
        if (included[team] != 0) {
            result.add(team);
        }
    }

    return result;
}

void CompiledLevelEvents::add_spawn_group(EntityType const type,
                                          std::int32_t offset,
                                          std::int32_t count) {
    assert(offset >= 0 && count >= 0);

    constexpr auto max_count{std::numeric_limits<LevelEventCount::value_type>::max()};
    while (count > 0) {
        auto const added{std::min(count, static_cast<std::int32_t>(max_count))};
        spawn_groups.add(
            type, offset, LevelEventCount{static_cast<LevelEventCount::value_type>(added)});
        offset += added;
        count -= added;
    }
}

auto CompiledLevelEvents::add_spawn_event(IndexSpan const groups) -> EventHandle {
    auto const index{static_cast<std::uint32_t>(spawn_events.size())};
    spawn_events.push_back(groups);
    return EventHandle{index, EventKind::Spawn};
}
auto CompiledLevelEvents::add_stop_event(telemetry::RunEndReason const reason) -> EventHandle {
    auto const index{static_cast<std::uint32_t>(stop_events.size())};
    stop_events.push_back(reason);
    return EventHandle{index, EventKind::StopRun};
}
auto CompiledLevelEvents::add_start_timer_event(StartTimerEvent const event) -> EventHandle {
    auto const index{static_cast<std::uint32_t>(start_timer_events.size())};
    start_timer_events.push_back(event);
    return EventHandle{index, EventKind::StartTimer};
}

auto CompiledLevelEvents::add_win_event(Team const team) -> EventHandle {
    auto const index{static_cast<std::uint32_t>(win_events.size())};
    win_events.push_back({team});
    return EventHandle{index, EventKind::Win};
}
void CompiledLevelEvents::add_trigger(ExprHandle const condition,
                                      std::span<EventHandle const> const events) {
    IndexSpan const range{static_cast<std::uint32_t>(trigger_events.size()),
                          static_cast<std::uint32_t>(events.size())};
    trigger_events.insert(trigger_events.end(), events.begin(), events.end());
    pending_triggers.push_back({condition, range});
}
}
