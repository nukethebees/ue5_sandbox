#include "SpaceGame/levels/LevelEventSchedule.h"

#include "LevelArchetypeResolution.h"
#include "LevelEntityTableOperations.h"
#include "LevelTeamResolution.h"

#include <SandboxCore/soa_rotator_utils.h>
#include <SpaceGame/levels/CompiledLevelEvents.h>

namespace ml {
namespace {
struct FLevelEventSource {
    uint64 execution_tick{};
    int32 source_index{};
};

struct FLevelMissionTickValues {
    TArray<int32> must_survive{};
    TArray<int32> required_kills{};
    TArray<int32> kill_target_increases{};
};

auto append_tick_mission_groups(FLevelEventSchedule& schedule,
                                FLevelMissionTickValues& values,
                                FString& error) -> bool {
    auto const append{[&](ELevelMissionEventType const type,
                          TConstArrayView<int32> const event_values,
                          TCHAR const* const name) {
        if (schedule.add_mission_group(type, event_values)) {
            return true;
        }

        error = FString::Printf(
            TEXT("Level event compilation at tick %llu: %s event count %d exceeds the "
                 "per-tick group limit of %d"),
            schedule.execution_ticks.Last(),
            name,
            event_values.Num(),
            static_cast<int32>(TNumericLimits<FLevelEventCount>::Max()));
        return false;
    }};
    if (!append(ELevelMissionEventType::MustSurvive, values.must_survive, TEXT("must-survive")) ||
        !append(
            ELevelMissionEventType::RequiredKill, values.required_kills, TEXT("required-kill")) ||
        !append(ELevelMissionEventType::IncreaseKillTarget,
                values.kill_target_increases,
                TEXT("kill-target increase"))) {
        return false;
    }

    values.must_survive.Reset();
    values.required_kills.Reset();
    values.kill_target_increases.Reset();
    return true;
}

auto append_tick_spawn_groups(FLevelEventSchedule& schedule,
                              int32& capital_offset,
                              int32& turret_offset,
                              FString& error) -> bool {
    auto const capital_end{schedule.capital_spawns.num()};
    auto const turret_end{schedule.turret_spawns.num()};
    auto const append{[&](ETestEntityType const type, int32 const offset, int32 const count) {
        if (schedule.add_spawn_group(type, offset, count)) {
            return true;
        }

        error = FString::Printf(
            TEXT("Level event compilation at tick %llu: %s spawn count %d exceeds the "
                 "per-tick group limit of %d"),
            schedule.execution_ticks.Last(),
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

void append_indices(TArray<int32>& output,
                    FLevelDefinition const& definition,
                    TConstArrayView<FLevelEntityId> const ids) {
    for (auto const id : ids) {
        output.Add(find_entity_index(definition, id));
    }
}
}

auto try_to_level_event_count(int32 const count, FLevelEventCount& result) -> bool {
    if (count < 0 || count > TNumericLimits<FLevelEventCount>::Max()) {
        return false;
    }
    result = static_cast<FLevelEventCount>(count);
    return true;
}

auto FLevelEventSchedule::add_spawn_group(ETestEntityType const type,
                                          int32 const offset,
                                          int32 const count) -> bool {
    if (count == 0) {
        return true;
    }
    check(!event_group_counts.IsEmpty());
    check(execution_ticks.Num() == event_group_counts.Num());

    FLevelEventCount payload_count;
    auto& tick_counts{event_group_counts.Last()};
    FLevelEventCount group_count;
    if (!try_to_level_event_count(count, payload_count) ||
        !try_to_level_event_count(tick_counts.spawn_groups + 1, group_count)) {
        return false;
    }

    tick_counts.spawn_groups = group_count;
    auto const index{spawn_groups.num()};
    spawn_groups.add_uninitialised(1);
    spawn_groups.set(index, type, offset, payload_count);
    return true;
}

auto FLevelEventSchedule::add_mission_group(ELevelMissionEventType const type,
                                            TConstArrayView<int32> const values) -> bool {
    auto const count{values.Num()};
    if (count == 0) {
        return true;
    }
    check(!event_group_counts.IsEmpty());
    check(execution_ticks.Num() == event_group_counts.Num());

    FLevelEventCount payload_count;
    auto& tick_counts{event_group_counts.Last()};
    FLevelEventCount group_count;
    if (!try_to_level_event_count(count, payload_count) ||
        !try_to_level_event_count(tick_counts.mission_groups + 1, group_count)) {
        return false;
    }

    tick_counts.mission_groups = group_count;
    auto& groups{mission_events.groups};
    auto const index{groups.num()};
    groups.add_uninitialised(1);
    groups.set(index, type, mission_events.values.Num(), payload_count);
    mission_events.values.Append(values.GetData(), count);
    return true;
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
    auto& mission_initialisation{initialisation.mission.Emplace()};
    mission_initialisation.level_id = definition.metadata.id.value;
    mission_initialisation.level_title = definition.metadata.title;
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
        auto const archetype{level_archetype_detail::resolve(entity.archetype)};
        auto const team{level_team_detail::resolve(entity.team)};
        check(archetype.IsSet() && team.IsSet());

        switch (archetype.GetValue()) {
            case level_archetype_detail::EResolvedArchetype::PlayerFighter: {
                break;
            }
            case level_archetype_detail::EResolvedArchetype::CapitalShip: {
                auto const event_index{capital_events.num()};
                capital_events.add_uninitialised(1);
                capital_events.set(event_index,
                                   entity_index,
                                   INDEX_NONE,
                                   FVector3f{entity.position},
                                   FRotator3f{entity.rotation},
                                   team.GetValue(),
                                   capital_config.max_health,
                                   0.f,
                                   capital_config.spawn_delay);
                break;
            }
            case level_archetype_detail::EResolvedArchetype::StaticTurret: {
                auto const event_index{turret_events.num()};
                turret_events.add_uninitialised(1);
                turret_events.set(event_index,
                                  entity_index,
                                  FVector3f{entity.position},
                                  FRotator3f{entity.rotation},
                                  team.GetValue(),
                                  turret_config.max_health,
                                  turret_config.laser.damage);
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

        if (schedule.execution_ticks.IsEmpty() ||
            schedule.execution_ticks.Last() != source.execution_tick) {
            if (!schedule.execution_ticks.IsEmpty()) {
                FString error;
                if (!append_tick_spawn_groups(schedule, capital_offset, turret_offset, error) ||
                    !append_tick_mission_groups(schedule, mission_values, error)) {
                    FLevelStartErrors errors;
                    errors.add(MoveTemp(error));
                    return FLevelEventCompilationResult{std::unexpect, MoveTemp(errors)};
                }
            }
            schedule.execution_ticks.Add(source.execution_tick);
            schedule.event_group_counts.AddDefaulted();
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
                mission_values.kill_target_increases.Add(event.kill_target_increase);
            }
        }
    }
    if (!schedule.execution_ticks.IsEmpty()) {
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
