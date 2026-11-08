#include <ioj/sim/levels/level_compilation.h>

#include <ioj/column_math.h>
#include <ioj/rotator_math.h>
#include <ioj/sim/expressions/expr_compilation_context.h>
#include <ioj/sim/levels/compiled_level_events.h>
#include <ioj/sim/sim_clock.h>
#include <ioj/transform3d.h>

#include <algorithm>
#include <array>
#include <concepts>
#include <limits>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace ioj::levels {
namespace {
struct EventSource {
    sim::TimerTicks execution_tick{};
    std::uint32_t source_index{};
};

auto entity_index(LevelDefinition const& definition, EntityId const& id) -> sim::LevelEntityRef {
    auto const& entity{definition.entity(id)};
    return sim::LevelEntityRef{static_cast<std::uint32_t>(&entity - definition.entities().data())};
}

} // namespace

auto compile_timer_duration(Duration const duration, sim::SimClock const& clock)
    -> std::expected<sim::TimerTicks, LevelCompilationErrors> {
    auto const ticks{duration.seconds() * clock.tick_rate().hz()};
    if (ticks > static_cast<double>(std::numeric_limits<sim::TimerTicks>::max())) {
        return std::unexpected{LevelCompilationErrors{"Timer duration exceeds tick range"}};
    }
    return clock.duration_to_tick_period<sim::TimerTicks>(duration).get();
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
    if (!definition.player_entity_id().empty()) {
        output.player_entity_ref = entity_index(definition, definition.player_entity_id());
    }

    auto const& logic{definition.logic()};
    std::unordered_set<EntityId> grouped;
    for (auto const& group : logic.spawn_groups) {
        grouped.insert(group.entities.begin(), group.entities.end());
    }

    std::vector<EventSource> sources;
    auto const entity_count{static_cast<std::uint32_t>(definition.entities().size())};
    sources.reserve(static_cast<std::size_t>(entity_count));
    for (std::uint32_t index{}; index < entity_count; ++index) {
        auto const& entity{definition.entities()[static_cast<std::size_t>(index)]};
        if (entity.archetype != EntityArchetype::PlayerFighter && !grouped.contains(entity.id)) {
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
                capital_events.healths()[row] =
                    entity.overrides.health.value_or(capital_config.max_health).get();
                capital_events.initial_fighter_spawn_delays()[row] =
                    entity.overrides.initial_spawn_delay.value_or(NonNegativeFinite<float>{});
                capital_events.fighter_spawn_cooldowns()[row] =
                    entity.overrides.spawn_cooldown.value_or(
                        NonNegativeFinite<float>::make(capital_config.spawn_delay));
            } else if (entity.archetype == EntityArchetype::StaticTurret) {
                auto const row{turret_end++};
                turret_events.entity_indices()[row] = entity_index_value;
                set_vector(turret_events.view_locations(), row, Vector3f{entity.position});
                set_rotation(turret_events.view_rotations(), row, Rotator3f{entity.rotation});
                turret_events.teams()[row] = entity.team;
                turret_events.healths()[row] =
                    entity.overrides.health.value_or(turret_config.max_health).get();
                turret_events.laser_damages()[row] = entity.overrides.laser_damage.value_or(
                    NonNegative<std::int32_t>::make(turret_config.laser.damage));
            } else if (entity.archetype == EntityArchetype::TubeSpinner) {
                auto const row{spinner_end++};
                spinner_events.entity_indices()[row] = entity_index_value;
                set_vector(spinner_events.view_locations(), row, Vector3f{entity.position});
                spinner_events.yaws()[row] = static_cast<float>(entity.rotation.yaw);
                spinner_events.initial_fire_point_indices()[row] =
                    entity.overrides.initial_fire_point.value_or(NonNegative<std::int32_t>{});
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
        if (tick == 0) {
            definitions.initial_events.push_back(spawn);
        } else {
            auto const timer{output.timers.add()};
            auto const elapsed{output.expressions.add_timer_elapsed(timer)};
            definitions.initial_events.push_back(definitions.add_start_timer_event({timer, tick}));
            definitions.add_trigger(elapsed, std::array{spawn});
        }
        first = end;
    }
    std::vector<sim::EventHandle> spawn_effects;
    for (auto const& group : logic.spawn_groups) {
        auto const capital_offset{capital_end};
        auto const turret_offset{turret_end};
        auto const spinner_offset{spinner_end};
        for (auto const& id : group.entities) {
            append_entity(entity_index(definition, id));
        }
        auto const first_group{definitions.spawn_groups.num()};
        definitions.add_spawn_group(
            sim::EntityType::CapitalShip, capital_offset, capital_end - capital_offset);
        definitions.add_spawn_group(
            sim::EntityType::Turret, turret_offset, turret_end - turret_offset);
        definitions.add_spawn_group(
            sim::EntityType::TubeSpinner, spinner_offset, spinner_end - spinner_offset);
        spawn_effects.push_back(definitions.add_spawn_event(
            {first_group, definitions.spawn_groups.num() - first_group}));
    }
    auto const timers{output.timers.add(logic.timer_count)};
    std::unordered_map<EntityId, sim::LevelEntityRef> entities;
    for (std::uint32_t i{}; i < entity_count; ++i) {
        entities.emplace(definition.entities()[i].id, sim::LevelEntityRef{i});
    }
    std::unordered_map<std::string, sim::SignalHandle> signals;
    auto register_signals{[&](std::span<Effect const> effects) {
        for (auto const& effect : effects) {
            if (auto const* publish{std::get_if<PublishEffect>(&effect)}) {
                signals.try_emplace(publish->signal,
                                    sim::SignalHandle{static_cast<std::uint32_t>(signals.size())});
            }
        }
    }};
    register_signals(logic.initial_effects);
    for (auto const& rule : logic.rules) {
        register_signals(rule.effects);
    }
    std::vector<sim::ExprHandle> expressions(logic.expressions.size());
    output.expressions.append(logic.expressions, {entities, timers, signals}, expressions);

    for (auto const& latch : logic.latches) {
        output.latches.add({expressions[latch.when], expressions[latch.value]});
    }
    auto compile_effects{[&](std::span<Effect const> effects)
                             -> std::expected<std::vector<sim::EventHandle>,
                                              LevelCompilationErrors> {
        std::vector<sim::EventHandle> result;
        for (auto const& value : effects) {
            auto compiled{std::visit(
                [&](auto const& effect) -> std::expected<sim::EventHandle, LevelCompilationErrors> {
                    using T = std::decay_t<decltype(effect)>;
                    if constexpr (std::same_as<T, WinEffect>) {
                        return definitions.add_win_event(effect.team);
                    } else if constexpr (std::same_as<T, StopEffect>) {
                        return definitions.add_stop_event(
                            sim::telemetry::RunEndReason::LevelCompleted);
                    } else if constexpr (std::same_as<T, PublishEffect>) {
                        return definitions.add_publish_event(signals.at(effect.signal));
                    } else if constexpr (std::same_as<T, SpawnEffect>) {
                        return spawn_effects[effect.group];
                    } else {
                        auto duration{
                            compile_timer_duration(Duration::make(effect.seconds.get()), clock)};
                        if (!duration) {
                            return std::unexpected{duration.error()};
                        }
                        return definitions.add_start_timer_event({timers[effect.timer], *duration});
                    }
                },
                value)};
            if (!compiled) {
                return std::unexpected{compiled.error()};
            }
            result.push_back(*compiled);
        }
        return result;
    }};
    auto initial{compile_effects(logic.initial_effects)};
    if (!initial) {
        return std::unexpected{initial.error()};
    }
    definitions.initial_events.insert(
        definitions.initial_events.end(), initial->begin(), initial->end());
    for (auto const& rule : logic.rules) {
        auto effects{compile_effects(rule.effects)};
        if (!effects) {
            return std::unexpected{effects.error()};
        }
        definitions.add_trigger(expressions[rule.when], *effects);
    }
    return {};
}
} // namespace levels
