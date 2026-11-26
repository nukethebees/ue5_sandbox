#include <SpaceGamePresentation/presentation/LevelPresentation.h>

#include <SpaceGamePresentation/entities/TestTeamVisualData.h>
#include <SpaceGameRendering/SparkRendererComponent.h>

#include <SandboxCoreEngine/VectorConversion.h>

#include <SandboxISMCComponent.h>

namespace ioj {
auto FLevelPresentationResources::is_valid() const -> bool {
    return IsValid(lasers) && IsValid(capital_ships) && IsValid(fighters) && IsValid(turrets) &&
           IsValid(spinners) && IsValid(sparks);
}
FLevelPresentation::FLevelPresentation(FLevelPresentationResources const& resources,
                                       presentation::MissionFrame const& frame,
                                       FGameAudioFacade audio)
    : config_{resources.config}
    , audio_{audio}
    , sparks{*resources.sparks}
    , lasers{*resources.lasers}
    , capital_ships{*resources.capital_ships}
    , capital_ship_fighters{*resources.fighters}
    , turrets{*resources.turrets}
    , spinners{*resources.spinners} {
    last_completed_tick_ = frame.status.completed_tick;
    auto const& config{config_};
    resources.sparks->initialise(config.sparks);
    sparks.clear();
    if (resources.player.IsSet() && frame.player.has_value()) {
        player_.Emplace(resources.player.GetValue(),
                        config.player_ship,
                        config.presentation.player_ship,
                        frame);
    }
#if WITH_EDITORONLY_DATA
    lasers.debug_drawer = config.laser_debug_drawer;
    lasers.debugging_shapes_enabled = config.laser_debug_shapes;
#endif
    capital_ship_fighters.enable_target_debug_drawing = config.fighter_debug_targets;
    capital_ship_fighters.enable_ship_location_debug_drawing = config.fighter_debug_locations;
    turrets.draw_target_arrows_enabled = config.turret_debug_targets;
    turrets.draw_debug_entity_info_enabled = config.turret_debug_entities;
    lasers.set_actor_config(&config.laser_projectiles);
    lasers.set_spark_effects(sparks);
    capital_ships.set_actor_config(&config.capital_ships, config.presentation.capital_ships);
    capital_ship_fighters.set_actor_config(&config.fighters);
    turrets.set_actor_config(&config.turrets, config.presentation.turrets);
    spinners.set_actor_config(&config.tube_spinners);

    auto const ValidateOptionalAssets{
        [](auto const&... systems) { (systems.ValidateOptionalAssets(), ...); }};
    ValidateOptionalAssets(capital_ships, turrets);

    lasers.player_colours_ =
        UTestTeamVisualData::build_team_colour_cache(config.player_ship.team_visual_data);
    lasers.fighter_colours_ =
        UTestTeamVisualData::build_team_colour_cache(config.fighters.team_visual_data);
    lasers.turret_colours_ =
        UTestTeamVisualData::build_team_colour_cache(config.turrets.team_visual_data);
    capital_ships.set_niagara_spawner(effects);
    lasers.clear_runtime_state_presentation();
    capital_ships.clear_runtime_state_presentation();
    capital_ship_fighters.clear_runtime_state_presentation();
    turrets.clear_runtime_state_presentation();
    spinners.clear_runtime_state_presentation();
    capital_ships.begin_play_presentation(frame);
    capital_ship_fighters.begin_play_presentation(frame);
    turrets.begin_play_presentation(frame);
    spinners.begin_play_presentation(frame);
    lasers.begin_play_presentation(frame.lasers, config.laser_instance_reserve);
}

void FLevelPresentation::tick(float const dt, presentation::MissionFrame const& frame) {
    auto const completed_tick{frame.status.completed_tick};
    if (last_completed_tick_ == completed_tick) {
        sparks.commit(dt);
        effects.update_spawns(dt, *lasers.instances->GetWorld());
        return;
    }

    last_completed_tick_ = completed_tick;
    for (auto const event : frame.events.player) {
        if (event == sim::player::PlayerEventKind::ShotFired) {
            audio_.play_player_laser();
        }
    }
    for (auto const location : frame.events.death_locations) {
        audio_.play_entity_death(FVector{to_unreal(location)});
    }
    if (player_.IsSet()) {
        player_->tick(frame);
    }
    capital_ships.update_visual_data(frame.events);
    turrets.update_visual_data(frame.events);
    capital_ship_fighters.update_visual_data(frame);
    spinners.update_visual_data(frame);
    lasers.update_visual_data(frame.lasers, &frame.events);
    capital_ships.end_tick_presentation(frame);
    turrets.end_tick_presentation(frame);
    spinners.end_tick_presentation(frame);
    lasers.end_tick_presentation();
    capital_ships.commit_visual_data();
    capital_ship_fighters.commit_visual_data(frame);
    turrets.commit_visual_data(frame);
    spinners.commit_visual_data();
    sparks.commit(dt);
    effects.update_spawns(dt, *lasers.instances->GetWorld());
}

} // namespace ioj
