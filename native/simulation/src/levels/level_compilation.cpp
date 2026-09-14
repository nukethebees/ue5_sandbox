#include <ioj/sim/levels/level_compilation.h>

#include <ioj/sim/levels/level_event_schedule.h>
#include <ioj/sim/rotator_types.h>
#include <ioj/sim/sim_clock.h>

#include <algorithm>
#include <cassert>
#include <limits>
#include <span>

namespace ioj::sim::levels {
namespace {
struct EventSource {
    ioj::sim::SimTick execution_tick{};
    std::int32_t source_index{};
};

struct MissionTickValues {
    std::vector<std::int32_t> must_survive{};
    std::vector<std::int32_t> required_kills{};
    std::vector<std::int32_t> kill_target_increases{};
};

auto entity_index(LevelDefinition const& definition, std::string const& id) -> std::int32_t {
    auto const found{std::ranges::find(definition.entities, id, &EntitySpawnDefinition::id)};
    assert(found != definition.entities.end());
    return static_cast<std::int32_t>(found - definition.entities.begin());
}

void append_indices(std::vector<std::int32_t>& output,
                    LevelDefinition const& definition,
                    std::vector<std::string> const& ids) {
    for (auto const& id : ids) {
        output.push_back(entity_index(definition, id));
    }
}

auto append_mission_groups(LevelEventSchedule& schedule,
                           MissionTickValues& values,
                           std::string& error) -> bool {
    auto const append = [&](ioj::sim::LevelMissionEventType const type,
                            std::span<std::int32_t const> const event_values,
                            std::string_view const name) {
        if (schedule.add_mission_group(type, event_values)) {
            return true;
        }
        error = "Level event compilation at tick " +
                std::to_string(schedule.execution_ticks.back()) + ": " + std::string{name} +
                " event count " + std::to_string(event_values.size()) +
                " exceeds the per-tick group limit of " +
                std::to_string(std::numeric_limits<ioj::sim::LevelEventCount>::max());
        return false;
    };
    if (!append(
            ioj::sim::LevelMissionEventType::MustSurvive, values.must_survive, "must-survive") ||
        !append(ioj::sim::LevelMissionEventType::RequiredKill,
                values.required_kills,
                "required-kill") ||
        !append(ioj::sim::LevelMissionEventType::IncreaseKillTarget,
                values.kill_target_increases,
                "kill-target increase")) {
        return false;
    }
    values.must_survive.clear();
    values.required_kills.clear();
    values.kill_target_increases.clear();
    return true;
}

auto append_spawn_groups(LevelEventSchedule& schedule,
                         std::int32_t& capital_offset,
                         std::int32_t& turret_offset,
                         std::string& error) -> bool {
    auto const capital_end{schedule.capital_spawns.num()};
    auto const turret_end{schedule.turret_spawns.num()};
    auto const append = [&](ioj::sim::EntityType const type,
                            std::int32_t const offset,
                            std::int32_t const count,
                            std::string_view const name) {
        if (schedule.add_spawn_group(type, offset, count)) {
            return true;
        }
        error = "Level event compilation at tick " +
                std::to_string(schedule.execution_ticks.back()) + ": " + std::string{name} +
                " spawn count " + std::to_string(count) + " exceeds the per-tick group limit of " +
                std::to_string(std::numeric_limits<ioj::sim::LevelEventCount>::max());
        return false;
    };
    if (!append(ioj::sim::EntityType::CapitalShip,
                capital_offset,
                capital_end - capital_offset,
                "CapitalShip") ||
        !append(
            ioj::sim::EntityType::Turret, turret_offset, turret_end - turret_offset, "Turret")) {
        return false;
    }
    capital_offset = capital_end;
    turret_offset = turret_end;
    return true;
}

auto team(std::string const& id) -> ioj::sim::Team {
    if (id == "red") {
        return ioj::sim::Team::Red;
    }
    if (id == "green") {
        return ioj::sim::Team::Green;
    }
    if (id == "blue") {
        return ioj::sim::Team::Blue;
    }
    if (id == "orange") {
        return ioj::sim::Team::Orange;
    }
    if (id == "yellow") {
        return ioj::sim::Team::Yellow;
    }
    return ioj::sim::Team::White;
}
} // namespace

auto compile_level(LevelDefinition const& definition,
                   SimClock const& clock,
                   CapitalShipSimConfig const& capital_config,
                   TurretSimConfig const& turret_config) -> LevelCompilationResult {
    auto validation{validate_level(definition)};
    if (!validation) {
        LevelCompilationErrors errors;
        errors.reserve(validation.errors.size());
        for (auto& error : validation.errors) {
            errors.push_back(std::move(error.message));
        }
        return std::unexpected{std::move(errors)};
    }

    CompiledLevelEvents compiled;
    auto& initialisation{compiled.initialisation};
    auto& initial_spawns{compiled.initial_spawns};
    auto& schedule{compiled.schedule};
    auto& mission_initialisation{initialisation.mission.emplace()};
    mission_initialisation.level_id = definition.metadata.id;
    mission_initialisation.level_title = definition.metadata.title;
    initialisation.entity_count = static_cast<std::int32_t>(definition.entities.size());
    if (!definition.player_entity_id.empty()) {
        initialisation.player_entity_index = entity_index(definition, definition.player_entity_id);
    }

    if (definition.mission) {
        auto const& mission{*definition.mission};
        mission_initialisation.mode = mission.mode;
        mission_initialisation.time_limit_seconds = mission.time_limit_seconds;
        mission_initialisation.kill_count = mission.kill_count;
        append_indices(
            mission_initialisation.hero_entity_indices, definition, mission.hero_entity_ids);
        append_indices(mission_initialisation.must_survive_entity_indices,
                       definition,
                       mission.must_survive_entity_ids);
        append_indices(mission_initialisation.required_kill_entity_indices,
                       definition,
                       mission.required_kill_entity_ids);
        if ((mission.mode == LevelMissionMode::KillEnemies ||
             mission.mode == LevelMissionMode::KillEnemiesWithinTime) &&
            !mission.kill_count && !mission.hero_entity_ids.empty()) {
            auto const hero_team{
                definition.entities[entity_index(definition, mission.hero_entity_ids.front())]
                    .team};
            std::int32_t enemy_count{};
            for (auto const& entity : definition.entities) {
                enemy_count += entity.team != hero_team ? 1 : 0;
            }
            mission_initialisation.kill_count = enemy_count;
        }
    }

    std::vector<EventSource> sources;
    auto const entity_count{static_cast<std::int32_t>(definition.entities.size())};
    auto const mission_event_count{static_cast<std::int32_t>(definition.mission_events.size())};
    sources.reserve(static_cast<std::size_t>(entity_count + mission_event_count));
    for (std::int32_t index{}; index < entity_count; ++index) {
        auto const& entity{definition.entities[static_cast<std::size_t>(index)]};
        if (entity.archetype != "player-fighter") {
            sources.push_back({clock.duration_to_tick_period(entity.spawn_time_seconds), index});
        }
    }
    for (std::int32_t index{}; index < mission_event_count; ++index) {
        auto const& event{definition.mission_events[static_cast<std::size_t>(index)]};
        if (!event.must_survive_entity_ids.empty() || !event.required_kill_entity_ids.empty() ||
            event.kill_target_increase > 0) {
            sources.push_back(
                {clock.duration_to_tick_period(event.time_seconds), entity_count + index});
        }
    }
    std::ranges::sort(sources, {}, [](EventSource const& source) {
        return std::pair{source.execution_tick, source.source_index};
    });

    std::int32_t capital_offset{};
    std::int32_t turret_offset{};
    MissionTickValues mission_values;
    auto const append_entity = [&](std::int32_t const entity_index_value,
                                   LevelCapitalSpawnEvents& capital_events,
                                   LevelTurretSpawnEvents& turret_events) {
        auto const& entity{definition.entities[static_cast<std::size_t>(entity_index_value)]};
        if (entity.archetype == "capital-ship") {
            auto const row{capital_events.num()};
            capital_events.add_uninitialised(1);
            capital_events.entity_indices[row] = entity_index_value;
            capital_events.target_entity_indices[row] = -1;
            capital_events.locations.set(row,
                                         ml::make_vector3f(static_cast<float>(entity.position.x),
                                                           static_cast<float>(entity.position.y),
                                                           static_cast<float>(entity.position.z)));
            capital_events.rotations.set(
                row,
                ioj::sim::Rotator3f{static_cast<float>(entity.rotation.pitch),
                                    static_cast<float>(entity.rotation.yaw),
                                    static_cast<float>(entity.rotation.roll)});
            capital_events.teams[row] = team(entity.team);
            capital_events.healths[row] = capital_config.max_health;
            capital_events.initial_fighter_spawn_delays[row] = 0.0f;
            capital_events.fighter_spawn_cooldowns[row] = capital_config.spawn_delay;
        } else if (entity.archetype == "static-turret") {
            auto const row{turret_events.num()};
            turret_events.add_uninitialised(1);
            turret_events.entity_indices[row] = entity_index_value;
            turret_events.locations.set(row,
                                        ml::make_vector3f(static_cast<float>(entity.position.x),
                                                          static_cast<float>(entity.position.y),
                                                          static_cast<float>(entity.position.z)));
            turret_events.rotations.set(
                row,
                ioj::sim::Rotator3f{static_cast<float>(entity.rotation.pitch),
                                    static_cast<float>(entity.rotation.yaw),
                                    static_cast<float>(entity.rotation.roll)});
            turret_events.teams[row] = team(entity.team);
            turret_events.healths[row] = turret_config.max_health;
            turret_events.laser_damages[row] = turret_config.laser.damage;
        }
    };

    for (auto const source : sources) {
        auto const is_entity{source.source_index < entity_count};
        if (is_entity && source.execution_tick == 0) {
            append_entity(
                source.source_index, initial_spawns.capital_spawns, initial_spawns.turret_spawns);
            continue;
        }
        if (schedule.execution_ticks.empty() ||
            schedule.execution_ticks.back() != source.execution_tick) {
            if (!schedule.execution_ticks.empty()) {
                std::string error;
                if (!append_spawn_groups(schedule, capital_offset, turret_offset, error) ||
                    !append_mission_groups(schedule, mission_values, error)) {
                    return std::unexpected{LevelCompilationErrors{std::move(error)}};
                }
            }
            schedule.execution_ticks.push_back(source.execution_tick);
            schedule.event_group_counts.emplace_back();
        }
        if (is_entity) {
            append_entity(source.source_index, schedule.capital_spawns, schedule.turret_spawns);
        } else {
            auto const& event{
                definition
                    .mission_events[static_cast<std::size_t>(source.source_index - entity_count)]};
            append_indices(mission_values.must_survive, definition, event.must_survive_entity_ids);
            append_indices(
                mission_values.required_kills, definition, event.required_kill_entity_ids);
            if (event.kill_target_increase > 0) {
                mission_values.kill_target_increases.push_back(event.kill_target_increase);
            }
        }
    }
    if (!schedule.execution_ticks.empty()) {
        std::string error;
        if (!append_spawn_groups(schedule, capital_offset, turret_offset, error) ||
            !append_mission_groups(schedule, mission_values, error)) {
            return std::unexpected{LevelCompilationErrors{std::move(error)}};
        }
    }
    return compiled;
}
} // namespace ioj::sim::levels
