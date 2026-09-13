#include "sandbox/simulation/levels/LevelEventSchedule.h"
#include <SpaceGameSimulation/entities/NativeEntityTypes.h>
#include <SpaceGameSimulation/simulation/NativeRotatorTypes.h>
#include <SpaceGameSimulation/simulation/NativeVectorTypes.h>

#include "LevelEntityTableOperations.h"

#include <SandboxCore/soa_rotator_utils.h>
#include <SpaceGame/levels/CompileLevelEvents.h>
#include <SpaceGame/levels/LevelEntityResolution.h>

namespace ml {
namespace {
struct FLevelEventSource {
    uint64 execution_tick{};
    int32 source_index{};
};

struct FLevelMissionTickValues {
    std::vector<int32> must_survive{};
    std::vector<int32> required_kills{};
    std::vector<int32> kill_target_increases{};
};

auto append_tick_mission_groups(FLevelEventSchedule& schedule,
                                FLevelMissionTickValues& values,
                                FString& error) -> bool {
    auto const append{[&](ml::simulation::LevelMissionEventType const type,
                          std::span<int32 const> const event_values,
                          TCHAR const* const name) {
        if (schedule.add_mission_group(type, event_values)) {
            return true;
        }

        error = FString::Printf(
            TEXT("Level event compilation at tick %llu: %s event count %d exceeds the "
                 "per-tick group limit of %d"),
            schedule.execution_ticks.back(),
            name,
            static_cast<int32>(event_values.size()),
            static_cast<int32>(TNumericLimits<FLevelEventCount>::Max()));
        return false;
    }};
    if (!append(ml::simulation::LevelMissionEventType::MustSurvive,
                values.must_survive,
                TEXT("must-survive")) ||
        !append(ml::simulation::LevelMissionEventType::RequiredKill,
                values.required_kills,
                TEXT("required-kill")) ||
        !append(ml::simulation::LevelMissionEventType::IncreaseKillTarget,
                values.kill_target_increases,
                TEXT("kill-target increase"))) {
        return false;
    }

    values.must_survive.clear();
    values.required_kills.clear();
    values.kill_target_increases.clear();
    return true;
}

auto append_tick_spawn_groups(FLevelEventSchedule& schedule,
                              int32& capital_offset,
                              int32& turret_offset,
                              FString& error) -> bool {
    auto const capital_end{schedule.capital_spawns.num()};
    auto const turret_end{schedule.turret_spawns.num()};
    auto const append{[&](ETestEntityType const type, int32 const offset, int32 const count) {
        if (schedule.add_spawn_group(ml::to_native(type), offset, count)) {
            return true;
        }

        error = FString::Printf(
            TEXT("Level event compilation at tick %llu: %s spawn count %d exceeds the "
                 "per-tick group limit of %d"),
            schedule.execution_ticks.back(),
            LexToString(type),
            count,
            static_cast<int32>(TNumericLimits<FLevelEventCount>::Max()));
        return false;
    }};
    if (!append(ETestEntityType::CapitalShip, capital_offset, capital_end - capital_offset) ||
        !append(ETestEntityType::Turret, turret_offset, turret_end - turret_offset)) {
        return false;
    }

    capital_offset = capital_end;
    turret_offset = turret_end;
    return true;
}

auto find_entity_index(FLevelDefinition const& definition, FLevelEntityId const id) -> int32 {
    auto const index{
        level_entity_table_detail::find_index(definition.entities.get_const_view(), id)};
    check(index != INDEX_NONE);
    return index;
}

void append_indices(std::vector<int32>& output,
                    FLevelDefinition const& definition,
                    TConstArrayView<FLevelEntityId> const ids) {
    for (auto const id : ids) {
        output.push_back(find_entity_index(definition, id));
    }
}
}

auto compile_level_events(FLevelDefinition const& definition,
                          FSimulationClock const& clock,
                          FCapitalSimulationConfig const& capital_config,
                          FTurretSimulationConfig const& turret_config)
    -> FLevelEventCompilationResult {
    auto const validation{validate_level(definition)};
    if (!validation) {
        FLevelStartErrors errors;
        for (auto const& error : validation.errors) {
            errors.add(error.message);
        }
        return FLevelEventCompilationResult{std::unexpect, MoveTemp(errors)};
    }

    FLevelEventCompilationResult result{std::in_place};
    auto& compiled{result.value()};
    auto& initialisation{compiled.initialisation};
    auto& initial_spawns{compiled.initial_spawns};
    auto& schedule{compiled.schedule};
    auto& mission_initialisation{initialisation.mission.emplace()};
    mission_initialisation.level_id = TCHAR_TO_UTF8(*definition.metadata.id.value.ToString());
    mission_initialisation.level_title = TCHAR_TO_UTF8(*definition.metadata.title);
    initialisation.entity_count = definition.entities.num();
    if (definition.player_entity_id.is_set()) {
        initialisation.player_entity_index =
            find_entity_index(definition, definition.player_entity_id);
    }

    auto const entities{definition.entities.get_const_view()};
    auto const entity_count{entities.num()};
    if (definition.mission.IsSet()) {
        auto const& mission{definition.mission.GetValue()};
        mission_initialisation.mode = mission.mode;
        if (mission.time_limit_seconds.IsSet()) {
            mission_initialisation.time_limit_seconds = mission.time_limit_seconds.GetValue();
        }
        if (mission.kill_count.IsSet()) {
            mission_initialisation.kill_count = mission.kill_count.GetValue();
        }
        append_indices(
            mission_initialisation.hero_entity_indices, definition, mission.hero_entity_ids);
        append_indices(mission_initialisation.must_survive_entity_indices,
                       definition,
                       mission.must_survive_entity_ids);
        append_indices(mission_initialisation.required_kill_entity_indices,
                       definition,
                       mission.required_kill_entity_ids);
        if ((mission.mode == ELevelMissionMode::KillEnemies ||
             mission.mode == ELevelMissionMode::KillEnemiesWithinTime) &&
            !mission.kill_count.IsSet() && !mission.hero_entity_ids.IsEmpty()) {
            auto const hero_index{find_entity_index(definition, mission.hero_entity_ids[0])};
            auto const hero_team{entities.teams[hero_index]};
            int32 enemy_count{};
            for (int32 entity_index{}; entity_index < entity_count; ++entity_index) {
                if (entities.teams[entity_index] != hero_team) {
                    ++enemy_count;
                }
            }
            mission_initialisation.kill_count = enemy_count;
        }
    }

    TArray<FLevelEventSource> sources;
    auto const mission_event_count{definition.mission_events.Num()};
    sources.Reserve(entity_count + mission_event_count);
    for (int32 entity_index{}; entity_index < entity_count; ++entity_index) {
        if (entities.archetypes[entity_index] != level_archetypes::player_fighter) {
            sources.Add({
                .execution_tick =
                    clock.duration_to_tick_period(entities.spawn_times_seconds[entity_index]),
                .source_index = entity_index,
            });
        }
    }
    for (int32 event_index{}; event_index < mission_event_count; ++event_index) {
        auto const& event{definition.mission_events[event_index]};
        if (!event.must_survive_entity_ids.IsEmpty() || !event.required_kill_entity_ids.IsEmpty() ||
            event.kill_target_increase > 0) {
            sources.Add({
                .execution_tick = clock.duration_to_tick_period(event.time_seconds),
                .source_index = entity_count + event_index,
            });
        }
    }
    sources.Sort([](FLevelEventSource const& lhs, FLevelEventSource const& rhs) {
        if (lhs.execution_tick != rhs.execution_tick) {
            return lhs.execution_tick < rhs.execution_tick;
        }
        return lhs.source_index < rhs.source_index;
    });

    int32 capital_offset{};
    int32 turret_offset{};
    FLevelMissionTickValues mission_values;
    auto const append_entity_spawn{[&](int32 const entity_index,
                                       FLevelCapitalSpawnEvents& capital_events,
                                       FLevelTurretSpawnEvents& turret_events) {
        auto const entity{level_entity_table_detail::get(entities, entity_index)};
        auto const archetype{resolve_level_archetype(entity.archetype)};
        auto const team{resolve_level_team(entity.team)};
        check(archetype.IsSet() && team.IsSet());

        switch (archetype.GetValue()) {
            case EResolvedLevelArchetype::PlayerFighter: {
                break;
            }
            case EResolvedLevelArchetype::CapitalShip: {
                auto const row{capital_events.num()};
                capital_events.add_uninitialised(1);
                capital_events.entity_indices[row] = entity_index;
                capital_events.target_entity_indices[row] = INDEX_NONE;
                capital_events.locations.set(row, ml::to_native(FVector3f{entity.position}));
                capital_events.rotations.set(row, ml::to_native(FRotator3f{entity.rotation}));
                capital_events.teams[row] = ml::to_native(team.GetValue());
                capital_events.healths[row] = capital_config.max_health;
                capital_events.initial_fighter_spawn_delays[row] = 0.f;
                capital_events.fighter_spawn_cooldowns[row] = capital_config.spawn_delay;
                break;
            }
            case EResolvedLevelArchetype::StaticTurret: {
                auto const row{turret_events.num()};
                turret_events.add_uninitialised(1);
                turret_events.entity_indices[row] = entity_index;
                turret_events.locations.set(row, ml::to_native(FVector3f{entity.position}));
                turret_events.rotations.set(row, ml::to_native(FRotator3f{entity.rotation}));
                turret_events.teams[row] = ml::to_native(team.GetValue());
                turret_events.healths[row] = turret_config.max_health;
                turret_events.laser_damages[row] = turret_config.laser.damage;
                break;
            }
        }
    }};
    for (auto const source : sources) {
        auto const is_entity_source{source.source_index < entity_count};
        if (is_entity_source && source.execution_tick == 0) {
            append_entity_spawn(
                source.source_index, initial_spawns.capital_spawns, initial_spawns.turret_spawns);
            continue;
        }

        if (schedule.execution_ticks.empty() ||
            schedule.execution_ticks.back() != source.execution_tick) {
            if (!schedule.execution_ticks.empty()) {
                FString error;
                if (!append_tick_spawn_groups(schedule, capital_offset, turret_offset, error) ||
                    !append_tick_mission_groups(schedule, mission_values, error)) {
                    FLevelStartErrors errors;
                    errors.add(MoveTemp(error));
                    return FLevelEventCompilationResult{std::unexpect, MoveTemp(errors)};
                }
            }
            schedule.execution_ticks.push_back(source.execution_tick);
            schedule.event_group_counts.emplace_back();
        }
        if (is_entity_source) {
            append_entity_spawn(
                source.source_index, schedule.capital_spawns, schedule.turret_spawns);
        } else {
            auto const& event{definition.mission_events[source.source_index - entity_count]};

            append_indices(mission_values.must_survive, definition, event.must_survive_entity_ids);
            append_indices(
                mission_values.required_kills, definition, event.required_kill_entity_ids);

            if (event.kill_target_increase > 0) {
                mission_values.kill_target_increases.push_back(event.kill_target_increase);
            }
        }
    }
    if (!schedule.execution_ticks.empty()) {
        FString error;
        if (!append_tick_spawn_groups(schedule, capital_offset, turret_offset, error) ||
            !append_tick_mission_groups(schedule, mission_values, error)) {
            FLevelStartErrors errors;
            errors.add(MoveTemp(error));
            return FLevelEventCompilationResult{std::unexpect, MoveTemp(errors)};
        }
    }
    return result;
}
}
