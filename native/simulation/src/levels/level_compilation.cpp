#include <ioj/sim/levels/level_compilation.h>

#include <ioj/column_math.h>
#include <ioj/levels/mission_validation.h>
#include <ioj/rotator_math.h>
#include <ioj/sim/levels/compiled_level_events.h>
#include <ioj/sim/missions/mission.h>
#include <ioj/sim/sim_clock.h>
#include <ioj/transform3d.h>

#include <algorithm>
#include <array>
#include <limits>
#include <unordered_map>
#include <utility>

namespace ioj::levels {
namespace {
struct EventSource {
    sim::SimTick execution_tick{};
    std::uint32_t source_index{};
};

auto entity_index(LevelDefinition const& definition, EntityId const& id) -> sim::LevelEntityRef {
    auto const& entity{definition.entity(id)};
    return sim::LevelEntityRef{static_cast<std::uint32_t>(&entity - definition.entities().data())};
}

} // namespace

auto compile_timer_duration(Duration const duration, sim::SimClock const& clock)
    -> std::expected<sim::SimTick, LevelCompilationErrors> {
    auto const ticks{duration.seconds() * clock.tick_rate().hz()};
    if (ticks >= static_cast<double>(std::numeric_limits<sim::SimTick::value_type>::max())) {
        return std::unexpected{LevelCompilationErrors{"Timer duration exceeds tick range"}};
    }
    return sim::SimTick{clock.duration_to_tick_period(duration).get()};
}

auto compile_mission(MissionDefinition const& definition,
                     std::span<EntityId const> const entities,
                     sim::SimClock const& clock,
                     sim::ExprManager& expressions,
                     sim::Timers& timers,
                     sim::CompiledLevelEvents& events)
    -> std::expected<sim::Mission, LevelCompilationErrors> {
    auto errors{validate_mission(definition, entities)};
    if (!errors.empty()) {
        return std::unexpected{std::move(errors)};
    }

    auto const count{static_cast<std::uint16_t>(definition.objectives.size())};
    sim::Objectives objectives{count};
    std::unordered_map<ObjectiveName, sim::ObjectiveId> objective_ids;
    std::unordered_map<EntityId, sim::LevelEntityRef> entity_indices;
    auto const entity_count{entities.size()};
    for (std::uint32_t index{}; index < entity_count; ++index) {
        entity_indices.emplace(entities[index], sim::LevelEntityRef{index});
    }
    for (std::uint16_t index{}; index < count; ++index) {
        objective_ids.emplace(definition.objectives[index].id, sim::ObjectiveId{index});
    }

    auto const columns{objectives.compilation_view()};
    std::vector<sim::ObjectivePrerequisite> prerequisites;
    std::vector<sim::TimerHandle> timer_handles;
    std::vector<sim::EventHandle> activation_events;
    for (std::uint16_t row{}; row < count; ++row) {
        sim::ObjectiveId const owner{row};
        auto const& source{definition.objectives[row]};
        columns.identifiers[row] = source.id;
        columns.display_names[row] = source.title.empty() ? source.id.value : source.title;
        columns.assigned_teams[row] = source.team;
        columns.wins_level[row] = source.wins_level;

        prerequisites.clear();
        for (auto const& dependency : source.dependencies) {
            prerequisites.emplace_back(objective_ids.at(dependency.objective), dependency.outcome);
        }
        objectives.add_prerequisites(owner, prerequisites);

        timer_handles.assign(source.expressions.size(), sim::TimerHandle{});
        activation_events.clear();
        auto const expression_count{source.expressions.size()};
        for (std::size_t index{}; index < expression_count; ++index) {
            auto const* timer{std::get_if<TimeElapsedExpr>(&source.expressions[index].payload)};
            if (!timer) {
                continue;
            }
            auto const duration{
                compile_timer_duration(Duration::make(timer->seconds.get()), clock)};
            if (!duration) {
                return std::unexpected{duration.error()};
            }
            auto const handle{timers.add()};
            timer_handles[index] = handle;
            activation_events.push_back(events.add_start_timer_event({handle, *duration}));
        }

        auto const compiled{
            expressions.append(source.expressions, {entity_indices, objective_ids, timer_handles})};
        columns.leaf_ranges[row] = compiled.leaves;
        columns.composite_ranges[row] = compiled.composites;
        objectives.add_activation_events(owner, activation_events);
        for (auto const root : EnumTraits<ObjectiveOutcome>::values) {
            if (source.roots[root] != absent_expression) {
                columns.roots[row][root] = compiled.handles[source.roots[root]];
            }
        }
    }
    return sim::Mission{std::move(objectives)};
}

auto compile_level(LevelDefinition const& level, sim::LevelSimInitData& output)
    -> std::expected<void, LevelCompilationErrors> {
    auto const& definition{level};

    sim::SimClock clock;
    clock.initialise(output.clock_settings);
    auto const& capital_config{output.capital_ships};
    auto const& turret_config{output.turrets};
    auto& definitions{output.level_events};
    output.level_metadata = definition.metadata();
    output.entity_count = static_cast<std::uint32_t>(definition.entities().size());
    if (!definition.player_entity_id().empty()) {
        output.player_entity_ref = entity_index(definition, definition.player_entity_id());
    }

    if (definition.mission()) {
        std::vector<EntityId> entities;
        for (auto const& entity : definition.entities()) {
            entities.push_back(entity.id);
        }
        auto mission{compile_mission(*definition.mission(),
                                     entities,
                                     clock,
                                     output.expressions,
                                     output.timers,
                                     definitions)};
        if (!mission) {
            return std::unexpected{std::move(mission.error())};
        }
        output.mission = std::move(*mission);
    }

    std::vector<EventSource> sources;
    auto const entity_count{static_cast<std::uint32_t>(definition.entities().size())};
    sources.reserve(static_cast<std::size_t>(entity_count));
    for (std::uint32_t index{}; index < entity_count; ++index) {
        auto const& entity{definition.entities()[static_cast<std::size_t>(index)]};
        if (entity.archetype != EntityArchetype::PlayerFighter) {
            auto const tick{
                compile_timer_duration(Duration::make(entity.spawn_time_seconds), clock)};
            if (!tick) {
                return std::unexpected{tick.error()};
            }
            sources.push_back({*tick, index});
        }
    }
    std::ranges::sort(sources, [](EventSource const& left, EventSource const& right) {
        return left.execution_tick < right.execution_tick ||
               (left.execution_tick == right.execution_tick &&
                left.source_index < right.source_index);
    });

    std::int32_t capital_count{};
    std::int32_t turret_count{};
    std::int32_t spinner_count{};
    for (auto const& entity : definition.entities()) {
        capital_count += entity.archetype == EntityArchetype::CapitalShip ? 1 : 0;
        turret_count += entity.archetype == EntityArchetype::StaticTurret ? 1 : 0;
        spinner_count += entity.archetype == EntityArchetype::TubeSpinner ? 1 : 0;
    }
    definitions.capital_spawns.add_uninitialised(capital_count);
    definitions.turret_spawns.add_uninitialised(turret_count);
    definitions.spinner_spawns.add_uninitialised(spinner_count);
    auto const capital_events{definitions.capital_spawns.view()};
    auto const turret_events{definitions.turret_spawns.view()};
    auto const spinner_events{definitions.spinner_spawns.view()};
    std::int32_t capital_end{};
    std::int32_t turret_end{};
    std::int32_t spinner_end{};

    auto const append_entity =
        [&, capital_events, turret_events](sim::LevelEntityRef const entity_index_value) {
            auto const& entity{definition.entities()[entity_index_value.index()]};
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
            } else if (entity.archetype == EntityArchetype::TubeSpinner) {
                auto const row{spinner_end++};
                spinner_events.entity_indices()[row] = entity_index_value;
                set_vector(spinner_events.view_locations(), row, Vector3f{entity.position});
                spinner_events.yaws()[row] = static_cast<float>(entity.rotation.yaw);
                spinner_events.initial_fire_point_indices()[row] =
                    entity.overrides.initial_fire_point.value_or(0);
            }
        };

    auto const source_count{sources.size()};
    std::size_t first{};
    while (first < source_count) {
        auto const tick{sources[first].execution_tick};
        auto const capital_offset{capital_end};
        auto const turret_offset{turret_end};
        auto const spinner_offset{spinner_end};
        auto end{first};
        while (end < source_count && sources[end].execution_tick == tick) {
            append_entity(sim::LevelEntityRef{sources[end].source_index});
            ++end;
        }

        auto const group_offset{definitions.spawn_groups.num()};
        definitions.add_spawn_group(
            sim::EntityType::CapitalShip, capital_offset, capital_end - capital_offset);
        definitions.add_spawn_group(
            sim::EntityType::Turret, turret_offset, turret_end - turret_offset);
        definitions.add_spawn_group(
            sim::EntityType::TubeSpinner, spinner_offset, spinner_end - spinner_offset);
        auto const spawn{definitions.add_spawn_event(
            {group_offset, definitions.spawn_groups.num() - group_offset})};
        if (tick == sim::SimTick{}) {
            definitions.initial_events.push_back(spawn);
        } else {
            auto const timer{output.timers.add()};
            auto const elapsed{output.expressions.add_timer_elapsed(timer)};
            definitions.initial_events.push_back(definitions.add_start_timer_event({timer, tick}));
            definitions.add_trigger(elapsed, std::array{spawn});
        }
        first = end;
    }
    return {};
}
} // namespace levels
