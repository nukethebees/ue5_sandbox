#include <ioj/sim/levels/level_event_schedule.h>

#include <ioj/diagnostics.h>
#include <ioj/sim/expressions/expr_manager.h>
#include <ioj/team_array.h>

#include <algorithm>
#include <array>
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
auto LevelEventSchedule::collect_participating_teams(std::optional<Team> const player_team) const
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

namespace level_spawn_groups_detail {
void append(LevelSpawnGroups& storage,
            EntityType const type,
            std::int32_t offset,
            std::int32_t count) {
    assert(offset >= 0 && count >= 0);

    constexpr auto max_count{std::numeric_limits<LevelEventCount::value_type>::max()};
    while (count > 0) {
        auto const added{std::min(count, static_cast<std::int32_t>(max_count))};
        auto const index{storage.num()};
        storage.add_uninitialised(1);
        auto const groups{storage.view()};
        groups.types()[index] = type;
        groups.offsets()[index] = offset;
        groups.counts()[index] = LevelEventCount{static_cast<LevelEventCount::value_type>(added)};
        offset += added;
        count -= added;
    }
}
}

void LevelEventSchedule::add_initial_spawn_group(EntityType const type,
                                                 std::int32_t const offset,
                                                 std::int32_t const count) {
    level_spawn_groups_detail::append(initial_spawn_groups, type, offset, count);
}
void LevelEventSchedule::add_spawn_group(EntityType const type,
                                         std::int32_t const offset,
                                         std::int32_t const count) {
    level_spawn_groups_detail::append(spawn_groups, type, offset, count);
}

auto LevelEventSchedule::add_spawn_event(IndexSpan const groups) -> EventHandle {
    auto const index{static_cast<std::uint32_t>(spawn_events.size())};
    spawn_events.push_back(groups);
    return EventHandle{index, EventKind::Spawn};
}
auto LevelEventSchedule::add_stop_event(telemetry::RunEndReason const reason) -> EventHandle {
    auto const index{static_cast<std::uint32_t>(stop_events.size())};
    stop_events.push_back(reason);
    return EventHandle{index, EventKind::StopRun};
}
auto LevelEventSchedule::add_start_timer_event(StartTimerEvent const event) -> EventHandle {
    auto const index{static_cast<std::uint32_t>(start_timer_events.size())};
    start_timer_events.push_back(event);
    return EventHandle{index, EventKind::StartTimer};
}

void LevelEventSchedule::add_trigger(ExprHandle const condition,
                                     std::span<EventHandle const> const events) {
    IndexSpan const range{static_cast<std::uint32_t>(trigger_events.size()),
                          static_cast<std::uint32_t>(events.size())};
    trigger_events.insert(trigger_events.end(), events.begin(), events.end());
    pending_triggers.push_back({condition, range});
}
auto LevelEventSchedule::add_timed_trigger(ExprManager& expressions,
                                           Timers& timers,
                                           SimClock const& clock,
                                           Duration const duration,
                                           std::span<EventHandle const> const events)
    -> std::expected<void, std::vector<std::string>> {
    std::array<levels::MissionExpr, 1> const condition{levels::MissionExpr{
        levels::TimeElapsedExpr{NonNegativeFinite<double>::make(duration.seconds())}}};
    auto expression{expressions.append(condition, {{}, {}, clock, timers, *this})};
    if (!expression) {
        return std::unexpected{std::move(expression.error())};
    }
    add_trigger(expression->handles[0], events);
    initial_events.append_range(expression->start_events);
    return {};
}
auto LevelEventSchedule::add_level_duration_limit(ExprManager& expressions,
                                                  Timers& timers,
                                                  SimClock const& clock,
                                                  Duration const duration)
    -> std::expected<void, std::vector<std::string>> {
    std::array const events{add_stop_event(telemetry::RunEndReason::DurationReached)};
    return add_timed_trigger(expressions, timers, clock, duration, events);
}
}
