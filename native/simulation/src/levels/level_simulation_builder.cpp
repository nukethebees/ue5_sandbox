#include <ioj/sim/levels/level_simulation_builder.h>

#include <ioj/column_math.h>
#include <ioj/rotator_math.h>
#include <ioj/sim/collision/collision_grid.h>
#include <ioj/sim/collision/entity_world_bounds.h>
#include <ioj/sim/levels/fighter_spawn_slot_validation.h>
#include <ioj/sim/levels/level_compilation.h>
#include <ioj/sim/levels/simulation_settings.h>

#include <cmath>
#include <format>
#include <utility>

namespace ioj::sim {
namespace {
auto finite(Vector3f const value) -> bool {
    return std::isfinite(value.x()) && std::isfinite(value.y()) && std::isfinite(value.z());
}

auto finite(Transform3d const& transform) -> bool {
    auto const& rotation{transform.rotation};
    return finite(Vector3f{transform.location}) && finite(Vector3f{transform.scale}) &&
           std::isfinite(rotation.x()) && std::isfinite(rotation.y()) &&
           std::isfinite(rotation.z()) && std::isfinite(rotation.w());
}

auto settings_from_data(LevelSimInitData const& data) -> SimulationSettings {
    return {.clock = data.clock_settings,
            .player = data.player ? data.player->config : PlayerSimConfig{},
            .lasers = data.lasers,
            .capital_ships = data.capital_ships,
            .fighters = data.fighters,
            .turrets = data.turrets,
            .spinners = data.spinners,
            .overlap_response = data.collision.overlap_response};
}
}

auto validate_level_simulation_init_data(LevelSimInitData const& data)
    -> std::expected<void, LevelSimulationBuildErrors> {
    LevelSimulationBuildErrors errors;
    if (auto valid{validate_settings(settings_from_data(data))}; !valid) {
        for (auto const& error : valid.error()) {
            errors.push_back(std::format("{}: {}", error.node_path, error.message));
        }
    }
    if (!collision::is_configured(data.collision.grid_geometry)) {
        errors.emplace_back("Collision grid requires positive dimensions and finite cell sizes");
    } else if (!finite(collision::grid_half_size(data.collision.grid_geometry))) {
        errors.emplace_back("Collision grid extents must be finite");
    }
    for (auto const type : EnumTraits<EntityType>::values) {
        auto const centre{data.collision.entity_bounds.centre(type)};
        auto const extent{data.collision.entity_bounds.half_extents(type)};
        if (!finite(centre) || !finite(extent) || extent.x() < 0.f || extent.y() < 0.f ||
            extent.z() < 0.f) {
            errors.emplace_back("Entity collision bounds must be finite with nonnegative extents");
        }
    }
    auto const& capital{data.capital_ships};
    if (capital.fighter_spawn_slots !=
        static_cast<std::int32_t>(capital.fighter_spawn_slots_relative_transforms.size())) {
        errors.emplace_back("Fighter spawn slot count does not match the supplied transforms");
    }
    for (auto const& slot : capital.fighter_spawn_slots_relative_transforms) {
        if (!finite(slot)) {
            errors.emplace_back("Fighter spawn transforms must be finite");
        }
    }
    if (data.player) {
        auto const& player{*data.player};
        if (!finite(player.transform) || !finite(player.body_transform) ||
            !finite(player.left_socket) || !finite(player.right_socket) ||
            !finite(player.middle_socket)) {
            errors.emplace_back("Player geometry and socket transforms must be finite");
        }
    }
    if (errors.empty()) {
        for (auto const& error : levels::validate_fighter_spawn_slots(
                 capital, data.fighters, data.collision.entity_bounds)) {
            if (error.kind == levels::FighterSpawnSlotValidationErrorKind::IntersectsCapital) {
                errors.push_back(std::format("fighter spawn slot {} intersects the capital "
                                             "collision bounds plus fighter clearance",
                                             error.first_slot));
            } else {
                errors.push_back(std::format("fighter spawn slots {} and {} have overlapping "
                                             "collision bounds plus {} cm clearance",
                                             error.first_slot,
                                             error.second_slot,
                                             error.clearance));
            }
        }
    }
    if (!errors.empty()) {
        return std::unexpected{std::move(errors)};
    }

    auto check_spawn =
        [&](EntityType const type, Vector3f const position, Quaternion4f const rotation) {
            auto const bounds{collision::make_entity_world_bounds(
                data.collision.entity_bounds, type, position, rotation)};
            if (!finite(bounds.min) || !finite(bounds.max)) {
                errors.emplace_back("Entity collision bounds must be finite");
                return;
            }
            auto const half_size{collision::grid_half_size(data.collision.grid_geometry)};
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
    auto const capitals{data.level_events.capital_spawns.const_view()};
    auto const capital_count{capitals.num()};
    for (std::uint32_t row{}; row < capital_count; ++row) {
        check_spawn(EntityType::CapitalShip,
                    vector_at(capitals.view_locations(), row),
                    to_quaternion(rotation_at(capitals.view_rotations(), row)));
    }
    auto const turrets{data.level_events.turret_spawns.const_view()};
    auto const turret_count{turrets.num()};
    for (std::uint32_t row{}; row < turret_count; ++row) {
        check_spawn(EntityType::Turret,
                    vector_at(turrets.view_locations(), row),
                    to_quaternion(rotation_at(turrets.view_rotations(), row)));
    }
    auto const spinners{data.level_events.spinner_spawns.const_view()};
    auto const spinner_count{spinners.num()};
    auto const spinner_yaws{spinners.yaws()};
    for (std::uint32_t row{}; row < spinner_count; ++row) {
        auto const fire_point{spinners.initial_fire_point_indices()[row]};
        if (fire_point < 0 ||
            static_cast<std::size_t>(fire_point) >= data.spinners.fire_point_offsets.size()) {
            errors.emplace_back("Spinner initial fire point is outside its configured fire points");
        }
        check_spawn(EntityType::TubeSpinner,
                    vector_at(spinners.view_locations(), row),
                    to_quaternion(Rotator3f{0.f, spinner_yaws[row], 0.f}));
    }
    if (!errors.empty()) {
        return std::unexpected{std::move(errors)};
    }
    return {};
}

auto make_level_simulation_init_data(levels::LevelDefinition const& definition,
                                     LevelSimInitData inputs,
                                     std::optional<settings::TickSettings> const clock_override)
    -> LevelSimulationBuildResult {
    auto resolved{resolve_simulation_settings(definition.settings())};
    if (!resolved) {
        LevelSimulationBuildErrors errors;
        for (auto const& error : resolved.error()) {
            errors.push_back(std::format("{}: {}", error.node_path, error.message));
        }
        return std::unexpected{std::move(errors)};
    }
    if (clock_override) {
        resolved->clock = *clock_override;
        if (auto valid{validate_settings(*resolved)}; !valid) {
            LevelSimulationBuildErrors errors;
            for (auto const& error : valid.error()) {
                errors.push_back(std::format("{}: {}", error.node_path, error.message));
            }
            return std::unexpected{std::move(errors)};
        }
    }
    if (!definition.player_entity_id().empty()) {
        auto& player{inputs.player ? *inputs.player : inputs.player.emplace()};
        auto const& entity{definition.entity(definition.player_entity_id())};
        player.team = entity.team;
        player.transform.location = entity.position;
        player.transform.rotation = to_quaternion(entity.rotation);
    } else if (inputs.player) {
        return std::unexpected{LevelSimulationBuildErrors{
            "Player input was supplied for a level without a player entity"}};
    }
    apply_settings(*resolved, inputs);
    if (inputs.player) {
        auto const& entity{definition.entity(definition.player_entity_id())};
        if (entity.overrides.health) {
            auto const health{Health{*entity.overrides.health}};
            inputs.player->health = {health, health};
        }
    }
    auto compiled{levels::compile_level(definition, inputs)};
    if (!compiled) {
        return std::unexpected{std::move(compiled.error())};
    }
    if (auto valid{validate_level_simulation_init_data(inputs)}; !valid) {
        return std::unexpected{std::move(valid.error())};
    }
    return inputs;
}
}
