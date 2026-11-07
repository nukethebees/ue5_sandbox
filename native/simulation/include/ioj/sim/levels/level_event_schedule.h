#pragma once
#include <ioj/duration.h>
#include <ioj/sim/entities/team_list.h>
#include <ioj/sim/index_span.h>
#include <ioj/sim/levels/event_handle.h>
#include <ioj/sim/levels/event_trigger.h>
#include <ioj/sim/levels/level_capital_spawn_events.h>
#include <ioj/sim/levels/level_spawn_groups.h>
#include <ioj/sim/levels/level_spinner_spawn_events.h>
#include <ioj/sim/levels/level_turret_spawn_events.h>
#include <ioj/sim/telemetry/run_end_reason.h>

#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace ioj::sim {
class ExprManager;
struct SimClock;

struct LevelEventSchedule {
    void add_initial_spawn_group(EntityType type, std::int32_t offset, std::int32_t count);
    void add_spawn_group(EntityType type, std::int32_t offset, std::int32_t count);
    [[nodiscard]] auto add_spawn_event(IndexSpan groups) -> EventHandle;
    [[nodiscard]] auto add_stop_event(telemetry::RunEndReason reason) -> EventHandle;
    void add_trigger(ExprHandle condition, std::span<EventHandle const> events);
    auto add_timed_trigger(ExprManager& expressions,
                           SimClock const& clock,
                           Duration duration,
                           std::span<EventHandle const> events)
        -> std::expected<void, std::vector<std::string>>;
    auto
        add_level_duration_limit(ExprManager& expressions, SimClock const& clock, Duration duration)
            -> std::expected<void, std::vector<std::string>>;

    // Include future capital/turret spawns when configuring team-dependent subsystems.
    auto collect_participating_teams(std::optional<Team> player_team = {}) const -> TeamList;

    LevelSpawnGroups initial_spawn_groups{};
    LevelSpawnGroups spawn_groups{};
    LevelCapitalSpawnEvents capital_spawns{};
    LevelTurretSpawnEvents turret_spawns{};
    LevelSpinnerSpawnEvents spinner_spawns{};
    std::vector<EventTrigger> pending_triggers{};
    std::vector<EventHandle> trigger_events{};
    std::vector<IndexSpan> spawn_events{};
    std::vector<telemetry::RunEndReason> stop_events{};
};
}
