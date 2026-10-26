#include <ioj/sim/levels/level_compilation.h>

#include <ioj/column_math.h>
#include <ioj/rotator_math.h>
#include <ioj/sim/levels/level_event_schedule.h>
#include <ioj/sim/missions/compiled_mission.h>
#include <ioj/sim/sim_clock.h>
#include <ioj/transform3d.h>

#include <algorithm>
#include <format>
#include <limits>
#include <span>
#include <utility>

namespace ioj::levels {
namespace {
struct EventSource {
    sim::SimTick execution_tick{};
    std::uint32_t source_index{};
};

auto entity_index(LevelDefinition const& definition, EntityId const& id) -> sim::LevelEntityIndex {
    auto const& entity{definition.entity(id)};
    return static_cast<sim::LevelEntityIndex>(&entity - definition.entities().data());
}

auto append_spawn_groups(sim::LevelEventSchedule& schedule,
                         std::int32_t const capital_end,
                         std::int32_t const turret_end,
                         std::int32_t& capital_offset,
                         std::int32_t& turret_offset,
                         std::string& error) -> bool {
    auto const append = [&](sim::EntityType const type,
                            std::int32_t const offset,
                            std::int32_t const count,
                            std::string_view const name) {
        if (schedule.add_spawn_group(type, offset, count)) {
            return true;
        }
        error = std::format("Level event compilation at tick {}: {} spawn count {} "
                            "exceeds the per-tick group limit of {}",
                            schedule.execution_ticks.back().value(),
                            name,
                            count,
                            sim::max_level_event_count);
        return false;
    };
    if (!append(sim::EntityType::CapitalShip,
                capital_offset,
                capital_end - capital_offset,
                "CapitalShip") ||
        !append(sim::EntityType::Turret, turret_offset, turret_end - turret_offset, "Turret")) {
        return false;
    }
    capital_offset = capital_end;
    turret_offset = turret_end;
    return true;
}

} // namespace

auto compile_level(LevelDefinition const& level,
                   sim::SimClock const& clock,
                   sim::CapitalShipSimConfig const& capital_config,
                   sim::TurretSimConfig const& turret_config) -> LevelCompilationResult {
    auto const& definition{level};

    sim::CompiledLevel compiled;
    auto& initialisation{compiled.events.initialisation};
    auto& schedule{compiled.events.schedule};
    compiled.result_metadata.level_id = definition.metadata().id.value;
    compiled.result_metadata.level_title = definition.metadata().title;
    compiled.result_metadata.par_time_seconds = definition.metadata().par_time_seconds;
    initialisation.entity_count = static_cast<std::uint32_t>(definition.entities().size());
    if (!definition.player_entity_id().empty()) {
        initialisation.player_entity_index =
            entity_index(definition, definition.player_entity_id());
    }

    if (definition.mission()) {
        std::vector<EntityId> entities;
        for (auto const& entity : definition.entities()) {
            entities.push_back(entity.id);
        }
        auto mission{sim::CompiledMission::compile(*definition.mission(), entities, clock)};
        if (!mission) {
            return std::unexpected{std::move(mission.error())};
        }
        compiled.mission = std::move(*mission);
    }

    std::vector<EventSource> sources;
    auto const entity_count{static_cast<std::uint32_t>(definition.entities().size())};
    sources.reserve(static_cast<std::size_t>(entity_count));
    for (std::uint32_t index{}; index < entity_count; ++index) {
        auto const& entity{definition.entities()[static_cast<std::size_t>(index)]};
        if (entity.archetype != EntityArchetype::PlayerFighter) {
            sources.push_back(
                {sim::SimTick{
                     clock.duration_to_tick_period(Duration::make(entity.spawn_time_seconds))
                         .get()},
                 index});
        }
    }
    std::ranges::sort(sources, [](EventSource const& left, EventSource const& right) {
        return left.execution_tick < right.execution_tick ||
               (left.execution_tick == right.execution_tick &&
                left.source_index < right.source_index);
    });

    std::int32_t capital_count{};
    std::int32_t turret_count{};
    for (auto const& entity : definition.entities()) {
        capital_count += entity.archetype == EntityArchetype::CapitalShip ? 1 : 0;
        turret_count += entity.archetype == EntityArchetype::StaticTurret ? 1 : 0;
    }
    schedule.capital_spawns.add_uninitialised(capital_count);
    schedule.turret_spawns.add_uninitialised(turret_count);
    auto const capital_events{schedule.capital_spawns.get_view()};
    auto const turret_events{schedule.turret_spawns.get_view()};
    std::int32_t capital_end{};
    std::int32_t turret_end{};

    auto const append_entity =
        [&, capital_events, turret_events](sim::LevelEntityIndex const entity_index_value) {
            auto const& entity{definition.entities()[static_cast<std::size_t>(entity_index_value)]};
            if (entity.archetype == EntityArchetype::CapitalShip) {
                auto const row{capital_end++};
                capital_events.entity_indices()[row] = entity_index_value;
                capital_events.target_entity_indices()[row] =
                    entity.overrides.target_entity_id.empty()
                        ? sim::invalid_level_entity_index
                        : entity_index(definition, entity.overrides.target_entity_id);
                set_vector(capital_events.view_locations(), row, Vector3f{entity.position});
                set_rotation(capital_events.view_rotations(), row, Rotator3f{entity.rotation});
                capital_events.teams()[row] = entity.team;
                capital_events.healths()[row] = sim::Health{
                    entity.overrides.health.value_or(capital_config.max_health.get().value())};
                capital_events.initial_fighter_spawn_delays()[row] =
                    entity.overrides.initial_spawn_delay.value_or(0.0f);
                capital_events.fighter_spawn_cooldowns()[row] =
                    entity.overrides.spawn_cooldown.value_or(capital_config.spawn_delay);
            } else if (entity.archetype == EntityArchetype::StaticTurret) {
                auto const row{turret_end++};
                turret_events.entity_indices()[row] = entity_index_value;
                set_vector(turret_events.view_locations(), row, Vector3f{entity.position});
                set_rotation(turret_events.view_rotations(), row, Rotator3f{entity.rotation});
                turret_events.teams()[row] = entity.team;
                turret_events.healths()[row] = sim::Health{
                    entity.overrides.health.value_or(turret_config.max_health.get().value())};
                turret_events.laser_damages()[row] =
                    entity.overrides.laser_damage.value_or(turret_config.laser.damage);
            }
        };

    for (auto const source : sources) {
        if (source.execution_tick.value() != 0) {
            break;
        }
        if (source.source_index < entity_count) {
            append_entity(source.source_index);
        }
    }
    auto capital_offset{capital_end};
    auto turret_offset{turret_end};
    schedule.add_initial_spawn_group(sim::EntityType::CapitalShip, 0, capital_offset);
    schedule.add_initial_spawn_group(sim::EntityType::Turret, 0, turret_offset);

    for (auto const source : sources) {
        if (source.execution_tick.value() == 0) {
            continue;
        }
        if (schedule.execution_ticks.empty() ||
            schedule.execution_ticks.back() != source.execution_tick) {
            if (!schedule.execution_ticks.empty()) {
                std::string error;
                if (!append_spawn_groups(
                        schedule, capital_end, turret_end, capital_offset, turret_offset, error)) {
                    return std::unexpected{LevelCompilationErrors{std::move(error)}};
                }
            }
            schedule.execution_ticks.push_back(source.execution_tick);
            schedule.event_group_counts.emplace_back();
        }
        append_entity(source.source_index);
    }
    if (!schedule.execution_ticks.empty()) {
        std::string error;
        if (!append_spawn_groups(
                schedule, capital_end, turret_end, capital_offset, turret_offset, error)) {
            return std::unexpected{LevelCompilationErrors{std::move(error)}};
        }
    }
    return compiled;
}
} // namespace levels
