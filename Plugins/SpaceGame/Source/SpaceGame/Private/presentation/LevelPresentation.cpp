#include <SandboxISMCComponent.h>
#include <SpaceGame/presentation/LevelPresentation.h>
#include <SpaceGame/ships/player/TestSpaceShip.h>
#include <SpaceGame/simulation/LevelSimulation.h>

auto FLevelPresentationResources::is_valid() const -> bool {
    return IsValid(lasers) && IsValid(capital_ships) && IsValid(fighters) && IsValid(turrets) &&
           IsValid(spinners) && IsValid(config);
}
FLevelPresentation::FLevelPresentation(FLevelPresentationResources const& resources,
                                       FLevelSimulation& simulation,
                                       TArray<FTransform> turret_transforms)
    : lasers{*resources.lasers}
    , capital_ships{*resources.capital_ships}
    , capital_ship_fighters{*resources.fighters}
    , turrets{*resources.turrets}
    , spinners{*resources.spinners}
    , player_{resources.player} {
    auto const& config{*resources.config};
    auto const& settings{resources.settings};
#if WITH_EDITORONLY_DATA
    lasers.debug_drawer = settings.laser_debug_drawer;
    lasers.debugging_shapes_enabled = settings.laser_debug_shapes;
#endif
    capital_ships.debugging_shapes_enabled = settings.capital_debug_shapes;
    capital_ship_fighters.enable_target_debug_drawing = settings.fighter_debug_targets;
    capital_ship_fighters.enable_ship_location_debug_drawing = settings.fighter_debug_locations;
    turrets.draw_target_arrows_enabled = settings.turret_debug_targets;
    turrets.draw_debug_entity_info_enabled = settings.turret_debug_entities;
    lasers.set_actor_config(&config.laser_projectiles);
    capital_ships.set_actor_config(&config.capital_ships);
    capital_ship_fighters.set_actor_config(&config.fighters);
    turrets.set_actor_config(&config.turrets);
    spinners.set_actor_config(&config.tube_spinners);
    lasers.bind_simulation(*simulation.get_lasers());
    capital_ships.bind_simulation(*simulation.get_capital_ships());
    capital_ship_fighters.bind_simulation(*simulation.get_capital_ship_fighters());
    turrets.bind_simulation(*simulation.get_turrets());
    spinners.bind_simulation(*simulation.get_spinners());
    capital_ships.set_niagara_spawner(effects);
    lasers.clear_runtime_state_presentation();
    capital_ships.clear_runtime_state_presentation();
    capital_ship_fighters.clear_runtime_state_presentation();
    turrets.clear_runtime_state_presentation();
    spinners.clear_runtime_state_presentation();
    if (player_.IsValid()) {
        check(simulation.get_player_ship_simulation());
        player_->bind_simulation(*simulation.get_player_ship_simulation());
        player_->begin_play_presentation();
    }
    capital_ships.begin_play_presentation();
    capital_ship_fighters.begin_play_presentation();
    turrets.begin_play_presentation(MoveTemp(turret_transforms));
    spinners.begin_play_presentation();
    lasers.begin_play_presentation();
}
FLevelPresentation::~FLevelPresentation() {
    if (player_.IsValid()) {
        player_->unbind_simulation();
    }
}

void FLevelPresentation::update_visual_data(float dt) {
    if (player_.IsValid()) {
        player_->update_visual_data(dt);
    }
    capital_ships.update_visual_data();
    capital_ship_fighters.update_visual_data();
    turrets.update_visual_data();
    spinners.update_visual_data();
    lasers.update_visual_data();
}
void FLevelPresentation::commit_visual_data(float dt) {
    if (player_.IsValid()) {
        player_->commit_visual_data();
    }
    capital_ships.commit_visual_data();
    capital_ship_fighters.commit_visual_data();
    turrets.commit_visual_data();
    spinners.commit_visual_data();
    lasers.commit_visual_data();
    effects.update_spawns(dt, *lasers.instances->GetWorld());
}
void FLevelPresentation::handle_player_death() {
    if (player_.IsValid()) {
        player_->handle_simulation_death();
    }
}
