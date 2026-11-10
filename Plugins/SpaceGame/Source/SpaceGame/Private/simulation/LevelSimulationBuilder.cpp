#include "SpaceGame/simulation/LevelSimulationBuilder.h"

#include <ioj/sim/entities/entity_type.h>
#include <ioj/sim/levels/level_compilation.h>
#include <ioj/sim/levels/level_simulation_builder.h>
#include <SpaceGame/ships/player/TestSpaceShip.h>
#include <SpaceGame/simulation/LevelCollisionHost.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>
#include <SpaceGamePresentation/entities/TestTeamConversion.h>

#include <SandboxCoreEngine/RotatorConversion.h>
#include <SandboxCoreEngine/strings.h>
#include <SandboxCoreEngine/TransformConversion.h>
#include <SandboxCoreEngine/VectorConversion.h>

#include <Engine/StaticMesh.h>
#include <Engine/StaticMeshSocket.h>
#include <Engine/World.h>

#include <array>

namespace ioj::level_simulation_builder {

void modify_editor_actor(AActor& actor) {
#if WITH_EDITOR
    auto const* const world{actor.GetWorld()};
    if (IsValid(world) && !world->IsGameWorld()) {
        actor.Modify();
    }
#else
    static_cast<void>(actor);
#endif
}

void append_native_errors(sim::LevelSimulationBuildErrors const& messages,
                          FLevelStartErrors& errors) {
    for (auto const& message : messages) {
        errors.add(to_fstring(message));
    }
}

}

namespace ioj {
void configure_player_actor(ATestSpaceShip& player, USpaceGameLevelConfig const& config) {
    level_simulation_builder::modify_editor_actor(player);
    player.set_actor_config(&config.player_ship);
}

auto make_level_simulation_init_data(USpaceGameLevelConfig const& config,
                                     settings::TickSettings const& clock_settings,
                                     TOptional<sim::player::PlayerSpawnData> player,
                                     UStaticMesh const* const player_collision_mesh)
    -> FLevelSimBuildResult {
    FLevelStartErrors errors;
    auto const require_mesh{[&errors](UStaticMesh const* const mesh, TCHAR const* const name) {
        if (!IsValid(mesh)) {
            errors.add(FString::Printf(TEXT("%s is unavailable"), name));
        }
    }};
    require_mesh(config.capital_ships.mesh, TEXT("capital_ships.mesh"));
    require_mesh(config.fighters.mesh, TEXT("fighters.mesh"));
    require_mesh(config.turrets.mesh, TEXT("turrets.mesh"));
    require_mesh(config.tube_spinners.mesh, TEXT("tube_spinners.mesh"));

    if (player.IsSet()) {
        require_mesh(player_collision_mesh, TEXT("player ship mesh"));
    }
    if (errors.has_errors()) {
        return FLevelSimBuildResult{std::unexpect, MoveTemp(errors)};
    }

    FLevelSimBuildResult result{std::in_place};
    auto& data{result.value()};
    data.clock_settings = clock_settings;
    data.collision.overlap_response = config.overlap_response;
    data.lasers = config.laser_projectiles.simulation;
    data.capital_ships = config.capital_ships.simulation;
    data.capital_ships.fighter_spawn_slots_relative_transforms.clear();
    for (auto const& transform : config.capital_ships.fighter_spawn_slots_relative_transforms) {
        data.capital_ships.fighter_spawn_slots_relative_transforms.push_back(to_native(transform));
    }
    data.capital_ships.fighter_spawn_slots = static_cast<std::int32_t>(
        data.capital_ships.fighter_spawn_slots_relative_transforms.size());
    data.fighters = config.fighters.simulation;
    data.turrets = config.turrets.simulation;
    data.turrets.fire_point_offset = Vector3f{config.turrets.fire_point_offset.GetLocation()};
    data.spinners = config.tube_spinners.simulation;
    data.spinners.fire_point_offsets.clear();
    for (auto const& offset : config.tube_spinners.fire_point_offsets) {
        data.spinners.fire_point_offsets.push_back(
            {Vector3f{offset.GetLocation()}, Rotator3f{offset.Rotator()}});
    }
    if (player.IsSet()) {
        data.player.emplace(MoveTemp(player.GetValue()));
    }
    auto const* socket{config.fighters.mesh->FindSocket(TEXT("Gun"))};
    data.fighters.fire_point_distance =
        IsValid(socket) ? static_cast<float>(socket->RelativeLocation.Size()) : 0.f;
    auto const dimensions{config.collision_grid.calculate_grid_dimensions()};
    data.collision.grid_geometry = {{dimensions.X, dimensions.Y, dimensions.Z},
                                    Vector3f{config.collision_grid.cell_size}};

    FLevelCollisionHost::EntityMeshes meshes{};
    meshes[sim::EntityType::PlayerShip] = player_collision_mesh;
    meshes[sim::EntityType::CapitalShip] = config.capital_ships.mesh;
    meshes[sim::EntityType::Fighter] = config.fighters.mesh;
    meshes[sim::EntityType::Turret] = config.turrets.mesh;
    meshes[sim::EntityType::TubeSpinner] = config.tube_spinners.mesh;
    auto bounds{FLevelCollisionHost::extract_entity_bounds(meshes)};
    if (!bounds) {
        return FLevelSimBuildResult{std::unexpect, MoveTemp(bounds.error())};
    }
    data.collision.entity_bounds = MoveTemp(bounds.value());
    if (auto valid{sim::validate_level_simulation_init_data(data)}; !valid) {
        level_simulation_builder::append_native_errors(valid.error(), errors);
        return FLevelSimBuildResult{std::unexpect, MoveTemp(errors)};
    }
    return result;
}

auto make_level_simulation_init_data(USpaceGameLevelConfig const& config,
                                     settings::TickSettings const& clock_settings,
                                     levels::LevelDefinition const& definition,
                                     TOptional<sim::player::PlayerSpawnData> player,
                                     sim::collision::WorldAABBs static_bounds,
                                     UStaticMesh const* const player_collision_mesh)
    -> FLevelSimBuildResult {
    auto result{make_level_simulation_init_data(
        config, clock_settings, MoveTemp(player), player_collision_mesh)};
    if (!result) {
        return result;
    }
    auto& data{result.value()};
    data.collision.static_bounds = MoveTemp(static_bounds);
    auto native{sim::make_level_simulation_init_data(definition, std::move(data), clock_settings)};
    if (!native) {
        FLevelStartErrors errors;
        level_simulation_builder::append_native_errors(native.error(), errors);
        return FLevelSimBuildResult{std::unexpect, MoveTemp(errors)};
    }
    return FLevelSimBuildResult{std::in_place, std::move(*native)};
}

auto prepare_level_simulation(FLevelSimulationRequest const& request) -> FLevelSimBuildResult {
    auto const& config{request.config};
    auto const has_player{IsValid(request.player)};
    auto const presentation_enabled{request.launch_options.presentation_mode ==
                                    levels::PresentationMode::Visual};
    TArray<FString> messages;
    config.get_validation_errors(messages, presentation_enabled);
    FLevelStartErrors errors;
    errors.append(MoveTemp(messages));
    if (presentation_enabled && has_player && !config.player_ship.team_visual_data) {
        errors.add(TEXT("player_ship.team_visual_data is null"));
    }
    if (errors.has_errors()) {
        return FLevelSimBuildResult{std::unexpect, MoveTemp(errors)};
    }
    auto const needs_player{!request.definition.player_entity_id().empty()};
    auto const* player_source{has_player ? request.player : nullptr};
    if (!player_source && needs_player) {
        player_source = config.classes.player_ship_class.GetDefaultObject();
        if (!IsValid(player_source)) {
            errors.add(TEXT("Player geometry requires a configured player ship class"));
            return FLevelSimBuildResult{std::unexpect, MoveTemp(errors)};
        }
    }
    TOptional<sim::player::PlayerSpawnData> player;
    if (player_source) {
        player.Emplace(player_source->make_spawn_data());
    }
    auto const* const player_mesh{player_source ? player_source->get_collision_mesh() : nullptr};
    return make_level_simulation_init_data(
        config, request.clock_settings, request.definition, MoveTemp(player), {}, player_mesh);
}

auto configure_level_run(sim::LevelSimInitData& data, levels::LaunchOptions const& options)
    -> std::expected<void, FLevelStartErrors> {
    FLevelStartErrors errors;
    if (options.simulated_duration_seconds.has_value()) {
        auto const duration{Duration::make(*options.simulated_duration_seconds)};
        sim::SimClock clock;
        clock.initialise(data.clock_settings);
        auto const ticks{levels::compile_timer_duration(duration, clock)};
        if (!ticks) {
            level_simulation_builder::append_native_errors(ticks.error(), errors);
            return std::unexpected{MoveTemp(errors)};
        }
        auto& events{data.level_events};
        auto const timer{data.timers.add()};
        auto const elapsed{data.expressions.add_timer_elapsed(timer)};
        events.initial_events.push_back(events.add_start_timer_event({timer, *ticks}));
        std::array const effects{events.add_stop_event(sim::RunEndReason::DurationReached)};
        events.add_trigger(elapsed, effects);
    }
    return {};
}
}
