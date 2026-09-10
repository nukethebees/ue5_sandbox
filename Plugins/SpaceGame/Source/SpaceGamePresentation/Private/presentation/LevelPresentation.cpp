#include <SandboxISMCComponent.h>
#include <SpaceGamePresentation/entities/TestTeamVisualData.h>
#include <SpaceGamePresentation/presentation/LevelPresentation.h>

#include <SpaceGameRendering/SparkRendererComponent.h>

auto FLevelPresentationResources::is_valid() const -> bool {
    return IsValid(lasers) && IsValid(capital_ships) && IsValid(fighters) && IsValid(turrets) &&
           IsValid(spinners) && IsValid(sparks);
}
FLevelPresentation::FLevelPresentation(FLevelPresentationResources const& resources,
                                       FLevelReadView const& view,
                                       TArray<FTransform> turret_transforms)
    : config_{resources.config}
    , sparks{*resources.sparks}
    , lasers{*resources.lasers}
    , capital_ships{*resources.capital_ships}
    , capital_ship_fighters{*resources.fighters}
    , turrets{*resources.turrets}
    , spinners{*resources.spinners} {
    auto const& config{config_};
    resources.sparks->initialise(config.sparks);
    sparks.clear();
    if (resources.player.IsSet() && view.player.IsSet()) {
        player_.Emplace(resources.player.GetValue(), config.player_ship, view.player.GetValue());
    }
#if WITH_EDITORONLY_DATA
    lasers.debug_drawer = config.laser_debug_drawer;
    lasers.debugging_shapes_enabled = config.laser_debug_shapes;
#endif
    capital_ships.debugging_shapes_enabled = config.capital_debug_shapes;
    capital_ship_fighters.enable_target_debug_drawing = config.fighter_debug_targets;
    capital_ship_fighters.enable_ship_location_debug_drawing = config.fighter_debug_locations;
    turrets.draw_target_arrows_enabled = config.turret_debug_targets;
    turrets.draw_debug_entity_info_enabled = config.turret_debug_entities;
    lasers.set_actor_config(&config.laser_projectiles);
    lasers.set_spark_effects(sparks);
    capital_ships.set_actor_config(&config.capital_ships);
    capital_ship_fighters.set_actor_config(&config.fighters);
    turrets.set_actor_config(&config.turrets);
    spinners.set_actor_config(&config.tube_spinners);

    auto const ValidateOptionalAssets{
        [](auto const&... systems) { (systems.ValidateOptionalAssets(), ...); }};
    ValidateOptionalAssets(capital_ships, turrets);

    update_views(view, false);
    last_frame_sequence_ = view.frame_sequence;
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
    capital_ships.begin_play_presentation();
    capital_ship_fighters.begin_play_presentation();
    turrets.begin_play_presentation(MoveTemp(turret_transforms));
    spinners.begin_play_presentation();
    lasers.begin_play_presentation();
}

void FLevelPresentation::update_views(FLevelReadView const& view, bool const consume_changes) {
    capital_ships.view_ = view.capitals;
    capital_ship_fighters.view_ = view.fighters;
    turrets.view_ = view.turrets;
    spinners.view_ = view.spinners;
    lasers.view_ = view.lasers;
    if (!consume_changes) {
        capital_ships.view_.changes = {};
        capital_ships.view_.deaths = {};
        turrets.view_.changes = {};
        turrets.view_.death_locations = {};
        lasers.view_.hits = {};
        lasers.view_.hit_ticks = {};
        lasers.view_.hit_ordinals = {};
    }
}
void FLevelPresentation::tick(float const dt, FLevelReadView const& view) {
    update_views(view, last_frame_sequence_ != view.frame_sequence);
    last_frame_sequence_ = view.frame_sequence;
    last_completed_tick_ = view.clock->get_completed_ticks();
    ++tick_count_;
    if (player_.IsSet() && view.player.IsSet()) {
        player_->tick(view.player.GetValue());
    }
    capital_ships.update_visual_data();
    capital_ship_fighters.update_visual_data();
    turrets.update_visual_data();
    spinners.update_visual_data();
    lasers.update_visual_data();
    capital_ships.end_tick_presentation();
    capital_ship_fighters.end_tick_presentation();
    turrets.end_tick_presentation();
    spinners.end_tick_presentation();
    lasers.end_tick_presentation();
    capital_ships.commit_visual_data();
    capital_ship_fighters.commit_visual_data();
    turrets.commit_visual_data();
    spinners.commit_visual_data();
    sparks.commit(dt);
    effects.update_spawns(dt, *lasers.instances->GetWorld());
}
