#include <ioj/sim/levels/level_compilation.h>

#include <ioj/column_math.h>
#include <ioj/rotator_math.h>
#include <ioj/sim/levels/level_event_schedule.h>
#include <ioj/sim/missions/mission.h>
#include <ioj/sim/sim_clock.h>
#include <ioj/transform3d.h>

#include <algorithm>
#include <array>
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

} // namespace

auto compile_level(LevelDefinition const& level,
                   sim::SimClock const& clock,
                   sim::CapitalShipSimConfig const& capital_config,
                   sim::TurretSimConfig const& turret_config) -> LevelCompilationResult {
    auto const& definition{level};

    sim::CompiledLevel compiled;
    auto& initialisation{compiled.events.initialisation};
    auto& schedule{compiled.events.schedule};
    compiled.level_metadata = definition.metadata();
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
        auto mission{sim::Mission::compile(*definition.mission(),
                                           entities,
                                           clock,
                                           compiled.expressions,
                                           compiled.timers,
                                           schedule)};
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
    auto const capital_events{schedule.capital_spawns.view()};
    auto const turret_events{schedule.turret_spawns.view()};
    std::int32_t capital_end{};
    std::int32_t turret_end{};

    auto const append_entity =
        [&, capital_events, turret_events](sim::LevelEntityIndex const entity_index_value) {
            auto const& entity{definition.entities()[static_cast<std::size_t>(entity_index_value)]};
            if (entity.archetype == EntityArchetype::CapitalShip) {
                auto const row{capital_end++};
                capital_events.entity_indices()[row] = entity_index_value;
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

    auto const source_count{sources.size()};
    std::size_t first{};
    while (first < source_count) {
        auto const tick{sources[first].execution_tick};
        auto const capital_offset{capital_end};
        auto const turret_offset{turret_end};
        auto end{first};
        while (end < source_count && sources[end].execution_tick == tick) {
            append_entity(sources[end].source_index);
            ++end;
        }

        if (tick == sim::SimTick{}) {
            schedule.add_initial_spawn_group(
                sim::EntityType::CapitalShip, capital_offset, capital_end - capital_offset);
            schedule.add_initial_spawn_group(
                sim::EntityType::Turret, turret_offset, turret_end - turret_offset);
        } else {
            auto const seconds{
                definition.entities()[sources[first].source_index].spawn_time_seconds};
            auto const group_offset{schedule.spawn_groups.num()};
            schedule.add_spawn_group(
                sim::EntityType::CapitalShip, capital_offset, capital_end - capital_offset);
            schedule.add_spawn_group(
                sim::EntityType::Turret, turret_offset, turret_end - turret_offset);
            std::array const events{schedule.add_spawn_event(
                {group_offset, schedule.spawn_groups.num() - group_offset})};
            auto trigger{schedule.add_timed_trigger(
                compiled.expressions, compiled.timers, clock, Duration::make(seconds), events)};
            if (!trigger) {
                return std::unexpected{std::move(trigger.error())};
            }
        }
        first = end;
    }
    return compiled;
}
} // namespace levels
