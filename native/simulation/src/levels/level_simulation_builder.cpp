#include <ioj/sim/levels/level_simulation_builder.h>

#include <ioj/column_math.h>
#include <ioj/rotator_math.h>
#include <ioj/sim/collision/collision_grid.h>
#include <ioj/sim/collision/entity_world_bounds.h>
#include <ioj/sim/levels/level_compilation.h>
#include <ioj/sim/levels/simulation_settings.h>

#include <cmath>
#include <format>
#include <utility>

namespace ioj::sim {
namespace {
auto is_finite(Transform3d const& transform) -> bool {
    return is_finite(Vector3f{transform.location}) && is_finite(Vector3f{transform.scale}) &&
           is_finite(transform.rotation.components);
}

}

auto validate_playable_level(levels::LevelDefinition const& definition)
    -> LevelSimulationValidationResult {
    return make_level_simulation_init_data(definition, {}, {}).transform([](auto&&) {});
}

auto validate_entity_geometry(settings::Settings const& settings)
    -> LevelSimulationValidationResult {
    LevelSimInitData preview;
    preview.player.emplace();
    apply_settings(settings, preview);
    return validate_level_simulation_init_data(preview);
}

auto validate_level_simulation_init_data(LevelSimInitData const& data)
    -> LevelSimulationValidationResult {
    LevelSimulationBuildErrors errors;
    if (!collision::is_configured(data.collision.grid_spec)) {
        errors.emplace_back("Collision grid requires positive dimensions and finite cell sizes");
    } else if (!is_finite(collision::grid_half_size(data.collision.grid_spec))) {
        errors.emplace_back("Collision grid extents must be finite");
    }
    for (auto const type : EnumTraits<EntityType>::values) {
        auto const centre{data.collision.entity_bounds.centre(type)};
        auto const extent{data.collision.entity_bounds.half_extents(type)};
        if (!is_finite(centre) || !is_nonnegative_finite(extent)) {
            errors.emplace_back("Entity collision bounds must be finite with nonnegative extents");
        }
    }
    if (data.player) {
        auto const& player{*data.player};
        if (!is_finite(player.transform) || !is_finite(player.body_transform) ||
            !is_finite(player.left_socket) || !is_finite(player.right_socket) ||
            !is_finite(player.middle_socket)) {
            errors.emplace_back("Player geometry and socket transforms must be finite");
        }
    }
    if (!errors.empty()) {
        return LevelSimulationValidationResult{std::unexpect, std::move(errors)};
    }

    auto check_spawn =
        [&](EntityType const type, Vector3f const position, Quaternion4f const rotation) {
            auto const bounds{collision::make_entity_world_bounds(
                data.collision.entity_bounds, type, position, rotation)};
            if (!is_finite(bounds.min) || !is_finite(bounds.max)) {
                errors.emplace_back("Entity collision bounds must be finite");
                return;
            }
            auto const half_size{collision::grid_half_size(data.collision.grid_spec)};
            for (std::size_t axis{}; axis < 3; ++axis) {
                if (bounds.min[axis] < -half_size[axis] || bounds.min[axis] >= half_size[axis] ||
                    bounds.max[axis] > half_size[axis]) {
                    errors.emplace_back("Entity collision bounds are outside the collision grid");
                    return;
                }
            }
        };
    if (data.player) {
        check_spawn(EntityType::PlayerShip,
                    Vector3f{data.player->transform.location},
                    Quaternion4f{data.player->transform.rotation});
    }
    auto const fighters{data.level_events.fighter_spawns().const_view()};
    auto const fighter_locations{fighters.view_locations().resolve()};
    auto const fighter_directions{fighters.view_directions().resolve()};
    auto const fighter_count{fighters.num()};
    for (std::uint32_t row{}; row < fighter_count; ++row) {
        check_spawn(EntityType::Fighter,
                    fighter_locations[row],
                    to_quaternion(direction_to_rotation(fighter_directions[row])));
    }
    auto const capitals{data.level_events.capital_spawns().const_view()};
    auto const capital_count{capitals.num()};
    for (std::uint32_t row{}; row < capital_count; ++row) {
        check_spawn(EntityType::CapitalShip,
                    vector_at(capitals.view_locations(), row),
                    to_quaternion(rotation_at(capitals.view_rotations(), row)));
    }
    auto const turrets{data.level_events.turret_spawns().const_view()};
    auto const turret_count{turrets.num()};
    for (std::uint32_t row{}; row < turret_count; ++row) {
        check_spawn(EntityType::Turret,
                    vector_at(turrets.view_locations(), row),
                    to_quaternion(rotation_at(turrets.view_rotations(), row)));
    }
    auto const spinners{data.level_events.spinner_spawns().const_view()};
    auto const spinner_count{spinners.num()};
    auto const spinner_yaws{spinners.yaws()};
    auto const initial_fire_points{spinners.initial_fire_point_indices()};
    for (std::uint32_t row{}; row < spinner_count; ++row) {
        auto const fire_point{initial_fire_points[row]};
        if (static_cast<std::size_t>(fire_point.get()) >= data.spinners.fire_point_offsets.size()) {
            errors.emplace_back("Spinner initial fire point is outside its configured fire points");
        }
        check_spawn(EntityType::TubeSpinner,
                    vector_at(spinners.view_locations(), row),
                    to_quaternion(Rotator3f{0.f, spinner_yaws[row], 0.f}));
    }
    if (!errors.empty()) {
        return LevelSimulationValidationResult{std::unexpect, std::move(errors)};
    }
    return {};
}

auto make_level_simulation_init_data(levels::LevelDefinition const& definition,
                                     LevelSimInitData inputs,
                                     settings::Settings const& settings,
                                     LevelRunTiming timing) -> LevelSimulationBuildResult {
    auto resolved{validate_settings(settings)};
    if (!resolved) {
        LevelSimulationBuildErrors errors;
        for (auto const& error : resolved.error()) {
            errors.push_back(std::format("{}: {}", error.node_path, error.message));
        }
        return LevelSimulationBuildResult{std::unexpect, std::move(errors)};
    }
    if (!definition.player_entity_id().empty()) {
        auto& player{inputs.player ? *inputs.player : inputs.player.emplace()};
        auto const& entity{definition.entity(definition.player_entity_id())};
        player.team = entity.team;
        player.transform.location = entity.position;
        player.transform.rotation = to_quaternion(entity.rotation);
    } else if (inputs.player) {
        return LevelSimulationBuildResult{
            std::unexpect, 1, "Player input was supplied for a level without a player entity"};
    }
    apply_settings(settings, inputs);
    auto const grid{inputs.collision.grid_spec};
    auto const half_size{collision::grid_half_size(grid)};
    for (auto const& box : definition.static_collision()) {
        for (std::size_t axis{}; axis < 3; ++axis) {
            auto const min{box.min[axis]};
            auto const max{box.max[axis]};
            if (!std::isfinite(min) || !std::isfinite(max) || min > max || min < -half_size[axis] ||
                min >= half_size[axis] || max > half_size[axis]) {
                return LevelSimulationBuildResult{
                    std::unexpect,
                    1,
                    "Static collision box is invalid or outside the collision grid"};
            }
        }

        inputs.collision.static_bounds.add(box.min, box.max);
    }

    if (timing.time_scale) {
        inputs.clock_settings.time_scale = *timing.time_scale;
    }
    if (inputs.player) {
        auto const& entity{definition.entity(definition.player_entity_id())};
        if (entity.overrides.health) {
            auto const health{entity.overrides.health->get()};
            inputs.player->health = {health, health};
        }
    }
    auto compiled{levels::compile_level(definition, inputs)};
    if (!compiled) {
        return LevelSimulationBuildResult{std::unexpect, std::move(compiled.error())};
    }
    if (timing.duration) {
        auto ticks{
            levels::compile_timer_duration(*timing.duration, inputs.clock_settings.tick_rate)};
        if (!ticks) {
            return LevelSimulationBuildResult{std::unexpect, std::move(ticks.error())};
        }
        auto& events{inputs.level_events};
        auto const timer{inputs.timers.start_run_limit(*ticks)};
        auto const elapsed{inputs.conditions.add_timer_elapsed(timer)};
        std::array const effects{events.add_stop_event(RunEndReason::DurationReached)};
        events.add_trigger(elapsed, effects);
    }
    if (auto valid{validate_level_simulation_init_data(inputs)}; !valid) {
        return LevelSimulationBuildResult{std::unexpect, std::move(valid.error())};
    }
    return inputs;
}

auto make_level_simulation_init_data(levels::LevelDefinition const& definition,
                                     LevelSimInitData inputs,
                                     LevelRunTiming timing) -> LevelSimulationBuildResult {
    return make_level_simulation_init_data(
        definition, std::move(inputs), definition.settings(), timing);
}
}
