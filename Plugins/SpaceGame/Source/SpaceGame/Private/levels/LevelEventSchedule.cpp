#include "SpaceGame/levels/LevelEventSchedule.h"

#include "LevelArchetypeResolution.h"
#include "LevelEntityTableOperations.h"
#include "LevelTeamResolution.h"

#include <SpaceGame/levels/CompiledLevelEvents.h>
#include <SpaceGame/support/logging/SandboxLogCategories.h>

#include <SandboxCore/soa_rotator_utils.h>

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

void append_tick_mission_groups(FLevelEventSchedule& schedule, FLevelMissionTickValues& values) {
    schedule.add_mission_group(ELevelMissionEventType::MustSurvive, values.must_survive);
    schedule.add_mission_group(ELevelMissionEventType::RequiredKill, values.required_kills);
    schedule.add_mission_group(ELevelMissionEventType::IncreaseKillTarget,
                               values.kill_target_increases);
    values.must_survive.Reset();
    values.required_kills.Reset();
    values.kill_target_increases.Reset();
}

void append_tick_spawn_groups(FLevelEventSchedule& schedule,
                              int32& capital_offset,
                              int32& turret_offset) {
    auto const capital_end{schedule.capital_spawns.num()};
    auto const turret_end{schedule.turret_spawns.num()};
    schedule.add_spawn_group(
        ETestEntityType::CapitalShip, capital_offset, capital_end - capital_offset);
    schedule.add_spawn_group(ETestEntityType::Turret, turret_offset, turret_end - turret_offset);
    capital_offset = capital_end;
    turret_offset = turret_end;
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

auto to_level_event_count(int32 const count) -> FLevelEventCount {
    if (count < 0 || count > TNumericLimits<FLevelEventCount>::Max()) {
        UE_LOG(LogSandbox,
               Fatal,
               TEXT("Level event compilation: count %d exceeds the supported range "
                    "0..%d. Increase FLevelEventCount to support this schedule."),
               count,
               static_cast<int32>(TNumericLimits<FLevelEventCount>::Max()));
    }
    return static_cast<FLevelEventCount>(count);
}

void FLevelEventSchedule::add_spawn_group(ETestEntityType const type,
                                          int32 const offset,
                                          int32 const count) {
    if (count == 0) {
        return;
    }
    check(!event_group_counts.IsEmpty());
    check(execution_ticks.Num() == event_group_counts.Num());

    auto const payload_count{to_level_event_count(count)};
    auto& tick_counts{event_group_counts.Last()};
    tick_counts.spawn_groups = to_level_event_count(tick_counts.spawn_groups + 1);
    auto const index{spawn_groups.num()};
    spawn_groups.add_uninitialised(1);
    spawn_groups.types[index] = type;
    spawn_groups.offsets[index] = offset;
    spawn_groups.counts[index] = payload_count;
}

void FLevelEventSchedule::add_mission_group(ELevelMissionEventType const type,
                                            TConstArrayView<int32> const values) {
    auto const count{values.Num()};
    if (count == 0) {
        return;
    }
    check(!event_group_counts.IsEmpty());
    check(execution_ticks.Num() == event_group_counts.Num());

    auto const payload_count{to_level_event_count(count)};
    auto& tick_counts{event_group_counts.Last()};
    tick_counts.mission_groups = to_level_event_count(tick_counts.mission_groups + 1);
    auto& groups{mission_events.groups};
    auto const index{groups.num()};
    groups.add_uninitialised(1);
    groups.types[index] = type;
    groups.offsets[index] = mission_events.values.Num();
    groups.counts[index] = payload_count;
    mission_events.values.Append(values.GetData(), count);
}

auto compile_level_events(FLevelDefinition const& definition,
                          FSimulationClock const& clock,
                          FCapitalSimulationConfig const& capital_config,
                          FTurretSimulationConfig const& turret_config) -> FCompiledLevelEvents {
    check(validate_level(definition));

    FCompiledLevelEvents compiled;
    auto& initialisation{compiled.initialisation};
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
    for (auto const source : sources) {
        if (schedule.execution_ticks.IsEmpty() ||
            schedule.execution_ticks.Last() != source.execution_tick) {
            if (!schedule.execution_ticks.IsEmpty()) {
                append_tick_spawn_groups(schedule, capital_offset, turret_offset);
                append_tick_mission_groups(schedule, mission_values);
            }
            schedule.execution_ticks.Add(source.execution_tick);
            schedule.event_group_counts.AddDefaulted();
        }
        if (source.source_index < entity_count) {
            auto const entity_index{source.source_index};
            auto const entity{level_entity_table_detail::get(entities, entity_index)};
            auto const archetype{level_archetype_detail::resolve(entity.archetype)};
            auto const team{level_team_detail::resolve(entity.team)};
            check(archetype.IsSet() && team.IsSet());

            switch (archetype.GetValue()) {
                case level_archetype_detail::EResolvedArchetype::PlayerFighter: {
                    break;
                }
                case level_archetype_detail::EResolvedArchetype::CapitalShip: {
                    auto& events{schedule.capital_spawns};
                    auto const event_index{events.num()};
                    events.add_uninitialised(1);
                    events.entity_indices[event_index] = entity_index;
                    events.target_entity_indices[event_index] = INDEX_NONE;
                    events.locations.set(event_index, FVector3f{entity.position});
                    ml::assign(events.rotations, event_index, entity.rotation);
                    events.teams[event_index] = team.GetValue();
                    events.healths[event_index] = capital_config.max_health;
                    events.initial_fighter_spawn_delays[event_index] = 0.f;
                    events.fighter_spawn_cooldowns[event_index] = capital_config.spawn_delay;
                    break;
                }
                case level_archetype_detail::EResolvedArchetype::StaticTurret: {
                    auto& events{schedule.turret_spawns};
                    auto const event_index{events.num()};
                    events.add_uninitialised(1);
                    events.entity_indices[event_index] = entity_index;
                    events.locations.set(event_index, FVector3f{entity.position});
                    ml::assign(events.rotations, event_index, entity.rotation);
                    events.teams[event_index] = team.GetValue();
                    events.healths[event_index] = turret_config.max_health;
                    events.laser_damages[event_index] = turret_config.laser.damage;
                    break;
                }
            }

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
        append_tick_spawn_groups(schedule, capital_offset, turret_offset);
        append_tick_mission_groups(schedule, mission_values);
    }
    return compiled;
}
}
