#include "SpaceGamePresentation/presentation/HUDManager.h"
#include <SpaceGamePresentation/support/logging/PresentationLogCategories.h>

#include <SpaceGamePresentation/presentation/LevelActorSettings.h>
#include "SpaceGamePresentation/entities/TestTeamVisualData.h"
#include "SpaceGamePresentation/presentation/widgets/ShipHudWidget.h"
#include "SpaceGamePresentation/presentation/widgets/SimulationHudWidget.h"
#include "SpaceGamePresentation/support/mesh.h"
#include "SpaceGameSimulation/missions/TestMissionManager.h"
#include "SpaceGameSimulation/ships/player/TestSpaceShipSimulation.h"
#include "SpaceGameSimulation/support/logging/SandboxLogCategories.h"

#include <SandboxCore/timing.h>

#include <Algo/Sort.h>
#include <Blueprint/WidgetLayoutLibrary.h>
#include <Components/InstancedStaticMeshComponent.h>
#include <Engine/StaticMesh.h>
#include <GameFramework/PlayerController.h>
#include <Materials/MaterialInterface.h>
#include <ProfilingDebugging/CountersTrace.h>

#include <utility>

TRACE_DECLARE_INT_COUNTER(SandboxEntityOverlayCandidateCount,
                          TEXT("Sandbox/EntityOverlay/CandidateCount"));
TRACE_DECLARE_INT_COUNTER(SandboxEntityOverlayInvalidHealthCount,
                          TEXT("Sandbox/EntityOverlay/InvalidHealthCount"));
TRACE_DECLARE_INT_COUNTER(SandboxEntityOverlayUploadBytes,
                          TEXT("Sandbox/EntityOverlay/UploadBytes"));
TRACE_DECLARE_INT_COUNTER(SandboxRadarCandidateCount, TEXT("Sandbox/Radar/CandidateCount"));
TRACE_DECLARE_INT_COUNTER(SandboxRadarVisibleCount, TEXT("Sandbox/Radar/VisibleCount"));
TRACE_DECLARE_INT_COUNTER(SandboxRadarUploadBytes, TEXT("Sandbox/Radar/UploadBytes"));

void FHUDManager::initialise(FTestBatchGameUiUpdateFrequencies const& update_frequencies,
                             FTestMissionManager const& new_mission_manager,
                             FTestEntityRegistry const& new_entity_registry,
                             double const update_tick_rate,
                             ml::test_space_ship::Simulation const* const new_player_ship,
                             FLevelVisualConfig const& level_config,
                             FEntityOverlaySettings const& entity_overlay_settings,
                             FRadarSettings const& radar_settings,
                             UInstancedStaticMeshComponent* const soft_target_instances) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FHUDManager::initialise);
    update_timers.reset();
    mission_data_buffers = {};
    entity_count_data_buffers = {};
    kill_data_buffers = {};
    player_status_data_buffers = {};
    player_flight_data_buffers = {};
    selected_mapping_context.Reset();
    has_mission_data = false;
#if WITH_EDITOR
    sampled_speed_data_buffers = {};
    has_sampled_speed_data = false;
#endif

    check(update_tick_rate > 0.0);
    seconds_per_tick_ = static_cast<float>(1.0 / update_tick_rate);

    auto const periods{update_frequencies.to_array()};
    auto const n_periods{periods.Num()};
    for (int32 i{0}; i < n_periods; ++i) {
        if (!ml::valid_periods(periods[i])) {
            UE_LOG(LogSandboxUI, Fatal, TEXT("FHUDManager::initialise: Invalid update period."));
        }

        auto const tick_period{static_cast<FPeriodicTickCountdown8::counter_type>(
            FMath::CeilToInt64(periods[i] * update_tick_rate))};
        if (!ml::valid_periods(tick_period)) {
            UE_LOG(LogSandboxUI, Fatal, TEXT("FHUDManager::initialise: Invalid tick period."));
        }
        update_timers.add_started(tick_period);
    }

    mission_manager = &new_mission_manager;
    entity_registry = &new_entity_registry;
    player_ship = new_player_ship;
    entity_overlay_settings_ = entity_overlay_settings;
    soft_target_instances_ = soft_target_instances;
    warned_multiple_world_target_views_ = false;
    configure_world_soft_target_renderer();
    radar_settings_ = sanitize_radar_settings(radar_settings);
    radar_style_.grid_opacity = radar_settings_.grid_opacity;
    radar_style_.core_cell_radius = radar_settings_.core_cell_radius;
    radar_style_.combat_cell_radius = radar_settings_.combat_cell_radius;
    radar_style_.tactical_cell_radius = radar_settings_.tactical_cell_radius;
    radar_style_.strategic_cell_radius = radar_settings_.strategic_cell_radius;
    radar_style_.glyph_size_scale = radar_settings_.glyph_size_scale;
    radar_style_.objective_size_multiplier = radar_settings_.objective_size_multiplier;
    radar_style_.objective_ring_padding_pixels = radar_settings_.objective_ring_padding_pixels;
    radar_style_.objective_ring_thickness_pixels = radar_settings_.objective_ring_thickness_pixels;
    auto const fighter_radius{ml::get_mesh_sphere_bounds(*level_config.fighters.mesh)};
    auto const capital_radius{ml::get_mesh_sphere_bounds(*level_config.capital_ships.mesh)};
    entity_overlay_style_ = {
        .bar_size_pixels = FVector2f{entity_overlay_settings.bar_size_pixels},
        .screen_offset_pixels = FVector2f{entity_overlay_settings.screen_offset_pixels},
        .minimum_world_radius = FMath::Min(fighter_radius, capital_radius),
        .maximum_world_radius = FMath::Max(fighter_radius, capital_radius),
        .minimum_bar_scale = FMath::Max(entity_overlay_settings.minimum_bar_scale, 0.01f),
        .maximum_bar_scale =
            FMath::Max(entity_overlay_settings.maximum_bar_scale,
                       FMath::Max(entity_overlay_settings.minimum_bar_scale, 0.01f)),
        .inset_pixels = entity_overlay_settings.inset_pixels,
        .maximum_inset_height_ratio = entity_overlay_settings.maximum_inset_height_ratio,
        .objective_bar_height_scale =
            FMath::Clamp(entity_overlay_settings.objective_bar_height_scale, 1.0f, 2.0f),
        .objective_frame_pixels = entity_overlay_settings.objective_frame_pixels,
        .screen_edge_padding_pixels = entity_overlay_settings.screen_edge_padding_pixels,
        .background_color = entity_overlay_settings.background_color,
        .fill_color = entity_overlay_settings.fill_color,
    };
    entity_overlay_team_colours_ = {
        .capital_ship = UTestTeamVisualData::build_team_colour_cache(
            level_config.capital_ships.team_visual_data),
        .fighter =
            UTestTeamVisualData::build_team_colour_cache(level_config.fighters.team_visual_data),
        .turret =
            UTestTeamVisualData::build_team_colour_cache(level_config.turrets.team_visual_data),
    };
    entity_overlay_maximum_health_ = {
        .capital_ship = level_config.capital_ships.max_health,
        .fighter = level_config.fighters.health,
        .turret = level_config.turrets.max_health,
    };
    soft_target_selection_settings_ = {
        .acquisition_radius_pixels = entity_overlay_settings.soft_target.acquisition_radius_pixels,
        .retention_radius_pixels = entity_overlay_settings.soft_target.retention_radius_pixels,
        .centre_tie_radius_pixels = entity_overlay_settings.soft_target.centre_tie_radius_pixels,
        .switch_improvement_ratio = entity_overlay_settings.soft_target.switch_improvement_ratio,
        .range_transition_start_multiplier =
            entity_overlay_settings.soft_target.range_transition_start_multiplier,
        .minimum_indicator_radius_pixels =
            entity_overlay_settings.soft_target.minimum_radius_pixels,
        .maximum_indicator_radius_pixels =
            entity_overlay_settings.soft_target.maximum_radius_pixels,
        .bounds_padding_pixels = entity_overlay_settings.soft_target.bounds_padding_pixels,
    };
    soft_target_pulse_duration_ =
        FMath::Max(entity_overlay_settings.soft_target.pulse_duration, 0.0f);
    soft_target_fade_out_duration_ =
        FMath::Max(entity_overlay_settings.soft_target.fade_out_duration, 0.0f);
    top_killer_ids_buffer.Reset();
    entity_overlay_objective_roles_.Reset();
    top_killer_ids_buffer.Reserve(entity_registry->get_num_unique_ids_issued());
    check(mission_manager);
    check(entity_registry);
    state = EHUDManagerState::Active;

    collect_mission_data();
    collect_entity_count_data();
    collect_kill_data();
    collect_player_status_data();
    collect_player_flight_data();
#if WITH_EDITOR
    collect_sampled_speed_data();
#endif

    for (auto const& registration : registered_huds) {
        auto* const hud{registration.hud.Get()};
        check(IsValid(hud));
        hud->set_entity_overlay_style(entity_overlay_style_);
        auto* const ship_hud{registration.ship_hud.Get()};
        if (IsValid(ship_hud)) {
            ship_hud->set_radar_style(radar_style_);
        }
        if (entity_overlay_settings_.enabled) {
            hud->set_entity_overlay_frame_store(registration.entity_overlay_frame_store);
        } else {
            hud->set_entity_overlay_frame_store({});
        }
        if (IsValid(ship_hud)) {
            ship_hud->set_radar_frame_store(radar_settings_.enabled ? registration.radar_frame_store
                                                                    : FRadarFrameStorePtr{});
        }
        synchronise_hud(*hud);
    }
    for (auto& registration : registered_huds) {
        registration.soft_target = {};
        registration.soft_target_range_alpha = 0.0f;
        registration.soft_target_radius_pixels = 0.0f;
        registration.soft_target_world_units_per_pixel = 0.0f;
        registration.soft_target_pulse_remaining = 0.0f;
        registration.fading_soft_target = {};
        registration.fading_soft_target_range_alpha = 0.0f;
        registration.fading_soft_target_radius_pixels = 0.0f;
        registration.fading_soft_target_world_units_per_pixel = 0.0f;
        registration.fading_soft_target_visibility_remaining = 0.0f;
    }
    update_entity_overlays(0.0f);
    update_radars();
}
void FHUDManager::deactivate() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FHUDManager::deactivate);
    TRACE_COUNTER_SET(SandboxEntityOverlayCandidateCount, 0);
    TRACE_COUNTER_SET(SandboxEntityOverlayInvalidHealthCount, 0);
    TRACE_COUNTER_SET(SandboxEntityOverlayUploadBytes, 0);
    TRACE_COUNTER_SET(SandboxRadarCandidateCount, 0);
    TRACE_COUNTER_SET(SandboxRadarVisibleCount, 0);
    TRACE_COUNTER_SET(SandboxRadarUploadBytes, 0);
    update_timers.reset();
    clear_world_soft_targets();
    for (auto& registration : registered_huds) {
        auto* const hud{registration.hud.Get()};
        if (!IsValid(hud)) {
            continue;
        }
        hud->set_entity_overlay_frame_store({});
        if (auto* const ship_hud{registration.ship_hud.Get()}; IsValid(ship_hud)) {
            ship_hud->set_radar_frame_store({});
        }
    }
    registered_huds.Reset();
    player_ship = nullptr;
    mission_manager = nullptr;
    entity_registry = nullptr;
    mission_data_buffers = {};
    entity_count_data_buffers = {};
    kill_data_buffers = {};
    player_status_data_buffers = {};
    player_flight_data_buffers = {};
    top_killer_ids_buffer.Reset();
    entity_overlay_objective_roles_.Reset();
    has_mission_data = false;
    seconds_per_tick_ = 0.0f;
    soft_target_instances_ = nullptr;
    soft_target_mesh_vertical_radius_ = 0.0f;
    warned_multiple_world_target_views_ = false;
    state = EHUDManagerState::Disabled;
#if WITH_EDITOR
    sampled_speed_data_buffers = {};
    has_sampled_speed_data = false;
#endif
}

void FHUDManager::tick(FPeriodicTickCountdown8::counter_type const num_ticks) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FHUDManager::tick);
    check(num_ticks >= 0);
    if (state != EHUDManagerState::Active) {
        return;
    }

    check(mission_manager);
    check(entity_registry);

    auto const changes{collect_data(num_ticks)};
    update_huds(changes);
    update_entity_overlays(static_cast<float>(num_ticks) * seconds_per_tick_);
    update_radars();
}

void FHUDManager::force_sample() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FHUDManager::force_sample);
    if (state != EHUDManagerState::Active) {
        return;
    }

    check(mission_manager);
    check(entity_registry);

    ml::hud_manager::FDataChanges changes;
    changes.mission = collect_mission_data();
    changes.entity_counts = collect_entity_count_data();
    collect_kill_data();
    changes.player_status = collect_player_status_data();
    changes.player_flight = collect_player_flight_data();
#if WITH_EDITOR
    changes.sampled_speed = collect_sampled_speed_data();
#endif
    update_huds(changes);
    update_entity_overlays(0.0f);
    update_radars();
}

void FHUDManager::register_hud(USimulationHudWidget& hud) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FHUDManager::register_hud);
    check(IsValid(&hud));
    check(!registered_huds.ContainsByPredicate(
        [&hud](FRegisteredHud const& existing) { return existing.hud.Get() == &hud; }));

    auto& registration{registered_huds.Emplace_GetRef()};
    registration.hud = &hud;
    registration.ship_hud = Cast<UShipHudWidget>(&hud);
    registration.entity_overlay_frame_store =
        MakeShared<FEntityOverlayFrameStore, ESPMode::ThreadSafe>();
    hud.set_entity_overlay_style(entity_overlay_style_);
    if (state == EHUDManagerState::Active && entity_overlay_settings_.enabled) {
        hud.set_entity_overlay_frame_store(registration.entity_overlay_frame_store);
    } else {
        hud.set_entity_overlay_frame_store({});
    }
    if (auto* const ship_hud{registration.ship_hud.Get()}; IsValid(ship_hud)) {
        registration.radar_frame_store = MakeShared<FRadarFrameStore, ESPMode::ThreadSafe>();
        ship_hud->set_radar_style(radar_style_);
        ship_hud->set_radar_frame_store(state == EHUDManagerState::Active && radar_settings_.enabled
                                            ? registration.radar_frame_store
                                            : FRadarFrameStorePtr{});
    }
    if (state == EHUDManagerState::Active) {
        synchronise_hud(hud);
        // Registration can precede the first HUD tick, or follow new simulation spawns.
        if (entity_overlay_settings_.enabled || radar_settings_.enabled) {
            update_entity_overlay_objective_roles();
        }
        if (entity_overlay_settings_.enabled) {
            auto const render_world_target{
                registration.ship_hud.IsValid() &&
                registered_huds.IndexOfByPredicate([](FRegisteredHud const& candidate) {
                    return candidate.ship_hud.IsValid();
                }) == registered_huds.Num() - 1};
            update_entity_overlay(registration, 0.0f, render_world_target);
        }
        if (radar_settings_.enabled) {
            update_radar(registration);
        }
    }
}
void FHUDManager::unregister_hud(USimulationHudWidget& hud) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FHUDManager::unregister_hud);
    check(IsValid(&hud));

    auto const index{registered_huds.IndexOfByPredicate(
        [&hud](FRegisteredHud const& existing) { return existing.hud.Get() == &hud; })};
    check(index != INDEX_NONE);
    registered_huds.RemoveAt(index);
}

void FHUDManager::set_selected_mapping_context(FString const& context_name) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FHUDManager::set_selected_mapping_context);
    if (selected_mapping_context == context_name) {
        return;
    }

    selected_mapping_context = context_name;
}

auto FHUDManager::collect_data(FPeriodicTickCountdown8::counter_type const num_ticks)
    -> ml::hud_manager::FDataChanges {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FHUDManager::collect_data);
    ml::hud_manager::FDataChanges changes;
    changes.player_flight = collect_player_flight_data();
#if WITH_EDITOR
    changes.sampled_speed = collect_sampled_speed_data();
#endif

    update_timers.tick(num_ticks);
    if (update_timers.try_consume(FHUDUpdateTimerIndex::player_status)) {
        changes.player_status = collect_player_status_data();
    }
    if (update_timers.try_consume(FHUDUpdateTimerIndex::mission_status)) {
        changes.mission = collect_mission_data();
    }
    if (update_timers.try_consume(FHUDUpdateTimerIndex::entity_counts)) {
        changes.entity_counts = collect_entity_count_data();
        collect_kill_data();
    }
    return changes;
}

void FHUDManager::update_entity_overlays(float const delta_seconds) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FHUDManager::update_entity_overlays);
    TRACE_COUNTER_SET(SandboxEntityOverlayCandidateCount, 0);
    TRACE_COUNTER_SET(SandboxEntityOverlayInvalidHealthCount, 0);
    TRACE_COUNTER_SET(SandboxEntityOverlayUploadBytes, 0);
    clear_world_soft_targets();
    if (registered_huds.IsEmpty() ||
        (!entity_overlay_settings_.enabled && !radar_settings_.enabled)) {
        return;
    }

    update_entity_overlay_objective_roles();
    if (!entity_overlay_settings_.enabled) {
        return;
    }
    auto world_target_rendered{false};
    auto ship_hud_count{0};
    for (auto& registration : registered_huds) {
        if (registration.ship_hud.IsValid()) {
            ++ship_hud_count;
        }
        auto const render_world_target{!world_target_rendered && registration.ship_hud.IsValid()};
        update_entity_overlay(registration, delta_seconds, render_world_target);
        world_target_rendered |= render_world_target;
    }
    if (ship_hud_count > 1 && !warned_multiple_world_target_views_) {
        UE_LOG(LogSandboxUI,
               Warning,
               TEXT("FHUDManager: World soft targets use the first registered ship HUD; "
                    "multiple local views are not supported by the shared ISM renderer."));
        warned_multiple_world_target_views_ = true;
    }
}

void FHUDManager::update_entity_overlay_objective_roles() {
    check(mission_manager);
    check(entity_registry);

    auto const entity_count{entity_registry->get_num_elements()};
    entity_overlay_objective_roles_.Init(EEntityOverlayObjectiveRole::None, entity_count);
    if (!mission_manager->mission_running()) {
        return;
    }

    auto const assign_role = [this](TConstArrayView<FRegistryEntityHandle> const handles,
                                    EEntityOverlayObjectiveRole const role) {
        for (auto const handle : handles) {
            if (!entity_registry->is_valid_alive(handle)) {
                continue;
            }

            check(entity_overlay_objective_roles_.IsValidIndex(handle.index));
            entity_overlay_objective_roles_[handle.index] = role;
        }
    };
    assign_role(mission_manager->get_entity_handles_that_must_survive(),
                EEntityOverlayObjectiveRole::Defend);
    assign_role(mission_manager->get_entity_handles_required_to_kill(),
                EEntityOverlayObjectiveRole::Destroy);
}

void FHUDManager::update_entity_overlay(FRegisteredHud& registration,
                                        float const delta_seconds,
                                        bool const render_world_target) {
    TRACE_CPUPROFILER_EVENT_SCOPE(EntityOverlay::Collect);
    auto* const hud{registration.hud.Get()};
    if (!IsValid(hud)) {
        UE_LOG(LogSandboxUI, Error, TEXT("FHUDManager: Registered HUD is invalid."));
        return;
    }

    check(registration.entity_overlay_frame_store.IsValid());
    auto& frame{registration.entity_overlay_frame_store->next()};
    auto const clear_active_soft_target = [&registration] {
        registration.soft_target = {};
        registration.soft_target_range_alpha = 0.0f;
        registration.soft_target_radius_pixels = 0.0f;
        registration.soft_target_world_units_per_pixel = 0.0f;
        registration.soft_target_pulse_remaining = 0.0f;
    };
    auto const clear_fading_soft_target = [&registration] {
        registration.fading_soft_target = {};
        registration.fading_soft_target_range_alpha = 0.0f;
        registration.fading_soft_target_radius_pixels = 0.0f;
        registration.fading_soft_target_world_units_per_pixel = 0.0f;
        registration.fading_soft_target_visibility_remaining = 0.0f;
    };
    auto const clear_soft_target_state = [&clear_active_soft_target, &clear_fading_soft_target] {
        clear_active_soft_target();
        clear_fading_soft_target();
    };

    FEntityOverlayView overlay_view;
    if (!hud->try_get_entity_overlay_view(overlay_view)) {
        UE_LOG(LogSandboxUI, Warning, TEXT("FHUDManager: Entity overlay view is invalid."));
        frame.instances.Reset();
        clear_soft_target_state();
        registration.entity_overlay_frame_store->publish();
        return;
    }

    FVector camera_location{};
    FRotator camera_rotation{};
    auto* const controller{hud->GetOwningPlayer()};
    if (!IsValid(controller)) {
        UE_LOG(LogSandboxUI, Error, TEXT("FHUDManager: Entity overlay has no player controller."));
        frame.instances.Reset();
        clear_soft_target_state();
        registration.entity_overlay_frame_store->publish();
        return;
    }
    controller->GetPlayerViewPoint(camera_location, camera_rotation);

    check(entity_registry);
    FSoftTargetSelectionResult soft_target;
    if (registration.ship_hud.IsValid() && validate_player_ship_for_collection()) {
        auto const firing_transform{player_ship->get_middle_socket()};
        auto const camera_transform{FRotationMatrix{camera_rotation}};
        soft_target =
            select_soft_target(entity_registry->get_entity_data().get_const_view(),
                               entity_registry->get_generations(),
                               entity_overlay_objective_roles_,
                               {.view = overlay_view,
                                .aim_origin = FVector3f{firing_transform.GetLocation()},
                                .aim_direction = FVector3f{firing_transform.GetUnitAxis(EAxis::X)},
                                .camera_right = FVector3f{camera_transform.GetUnitAxis(EAxis::Y)},
                                .camera_up = FVector3f{camera_transform.GetUnitAxis(EAxis::Z)},
                                .player_team = player_ship->team,
                                .effective_weapon_range = player_ship->get_laser_effective_range(),
                                .maximum_overlay_range = entity_overlay_settings_.maximum_range},
                               soft_target_selection_settings_,
                               registration.soft_target);
    }

    auto const elapsed_seconds{FMath::Max(delta_seconds, 0.0f)};
    registration.soft_target_pulse_remaining =
        FMath::Max(registration.soft_target_pulse_remaining - elapsed_seconds, 0.0f);
    if (registration.fading_soft_target.is_valid()) {
        registration.fading_soft_target_visibility_remaining = FMath::Max(
            registration.fading_soft_target_visibility_remaining - elapsed_seconds, 0.0f);
        if (!entity_registry->is_valid_alive(registration.fading_soft_target) ||
            registration.fading_soft_target_visibility_remaining <= 0.0f) {
            clear_fading_soft_target();
        }
    }

    auto const begin_active_soft_target_fade = [&] {
        if (!registration.soft_target.is_valid() ||
            !entity_registry->is_valid_alive(registration.soft_target) ||
            soft_target_fade_out_duration_ <= 0.0f) {
            return;
        }
        registration.fading_soft_target = registration.soft_target;
        registration.fading_soft_target_range_alpha = registration.soft_target_range_alpha;
        registration.fading_soft_target_radius_pixels = registration.soft_target_radius_pixels;
        registration.fading_soft_target_world_units_per_pixel =
            registration.soft_target_world_units_per_pixel;
        registration.fading_soft_target_visibility_remaining = soft_target_fade_out_duration_;
    };

    if (soft_target.handle.is_valid()) {
        if (soft_target.handle != registration.soft_target) {
            if (soft_target.handle == registration.fading_soft_target) {
                clear_fading_soft_target();
            }
            begin_active_soft_target_fade();
            registration.soft_target_pulse_remaining = 0.0f;
        } else if (registration.soft_target_range_alpha < 1.0f && soft_target.range_alpha >= 1.0f) {
            registration.soft_target_pulse_remaining = soft_target_pulse_duration_;
        }
        registration.soft_target = soft_target.handle;
        registration.soft_target_range_alpha = FMath::Clamp(soft_target.range_alpha, 0.0f, 1.0f);
        registration.soft_target_radius_pixels = soft_target.indicator_radius_pixels;
        registration.soft_target_world_units_per_pixel = soft_target.world_units_per_pixel;
    } else {
        if (soft_target.previous_target_can_fade) {
            begin_active_soft_target_fade();
        }
        clear_active_soft_target();
    }

    auto const pulse{soft_target_pulse_duration_ > 0.0f
                         ? registration.soft_target_pulse_remaining / soft_target_pulse_duration_
                         : 0.0f};
    auto const fading_visibility{soft_target_fade_out_duration_ > 0.0f
                                     ? registration.fading_soft_target_visibility_remaining /
                                           soft_target_fade_out_duration_
                                     : 0.0f};

    if (render_world_target) {
        auto const neutral_color{hud->get_soft_target_neutral_colour()};
        auto const in_range_color{hud->get_soft_target_in_range_colour()};
        FWorldSoftTargetCustomDataBuffer custom_data;
        add_world_soft_target(registration.soft_target,
                              registration.soft_target_range_alpha,
                              registration.soft_target_radius_pixels,
                              registration.soft_target_world_units_per_pixel,
                              pulse,
                              1.0f,
                              camera_location,
                              camera_rotation,
                              neutral_color,
                              in_range_color,
                              custom_data);
        add_world_soft_target(registration.fading_soft_target,
                              registration.fading_soft_target_range_alpha,
                              registration.fading_soft_target_radius_pixels,
                              registration.fading_soft_target_world_units_per_pixel,
                              0.0f,
                              fading_visibility,
                              camera_location,
                              camera_rotation,
                              neutral_color,
                              in_range_color,
                              custom_data);

        if (!custom_data.IsEmpty()) {
            auto* const instances{soft_target_instances_.Get()};
            auto const instance_count{custom_data.Num() / ml::soft_target_world::custom_data_count};
            check(IsValid(instances));
            check(custom_data.Num() == instance_count * ml::soft_target_world::custom_data_count);
            check(instance_count == instances->GetInstanceCount());
            if (!instances->SetCustomData(0, instance_count - 1, custom_data, true)) {
                UE_LOG(LogSandboxUI,
                       Error,
                       TEXT("FHUDManager: Failed to upload world soft-target custom data."));
            }
        }
    }

    auto const result{
        collect_entity_overlay_instances(entity_registry->get_entity_data().get_const_view(),
                                         entity_overlay_objective_roles_,
                                         entity_overlay_team_colours_,
                                         entity_overlay_maximum_health_,
                                         overlay_view.camera_origin,
                                         entity_overlay_settings_.maximum_range,
                                         frame.instances,
                                         registration.entity_overlay_collector)};
    registration.entity_overlay_frame_store->publish();

    TRACE_COUNTER_SET(SandboxEntityOverlayCandidateCount, result.candidate_count);
    TRACE_COUNTER_SET(SandboxEntityOverlayInvalidHealthCount, result.invalid_health_count);
    TRACE_COUNTER_SET(SandboxEntityOverlayUploadBytes,
                      static_cast<int64>(result.candidate_count) * sizeof(FEntityOverlayInstance));
}

void FHUDManager::configure_world_soft_target_renderer() {
    if (!entity_overlay_settings_.enabled) {
        if (auto* const instances{soft_target_instances_.Get()}; IsValid(instances)) {
            instances->ClearInstances();
            instances->SetVisibility(false);
        }
        soft_target_mesh_vertical_radius_ = 0.0f;
        return;
    }

    auto* const instances{soft_target_instances_.Get()};
    if (!IsValid(instances)) {
        return;
    }

    instances->ClearInstances();
    instances->SetVisibility(false);
    soft_target_mesh_vertical_radius_ = 0.0f;
    if (!IsValid(entity_overlay_settings_.soft_target.mesh) ||
        !IsValid(entity_overlay_settings_.soft_target.material)) {
        UE_LOG(LogSandboxUI,
               Error,
               TEXT("FHUDManager: World soft-target mesh or material is invalid."));
        return;
    }

    instances->SetMobility(EComponentMobility::Movable);
    instances->SetStaticMesh(entity_overlay_settings_.soft_target.mesh);
    auto const material_count{instances->GetNumMaterials()};
    for (int32 material_index{0}; material_index < material_count; ++material_index) {
        instances->SetMaterial(material_index, entity_overlay_settings_.soft_target.material);
    }
    instances->SetNumCustomDataFloats(ml::soft_target_world::custom_data_count);
    instances->SetCanEverAffectNavigation(false);
    instances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    instances->SetGenerateOverlapEvents(false);
    instances->SetCastShadow(false);
    instances->SetAffectDistanceFieldLighting(false);
    instances->SetReceivesDecals(false);
    instances->SetRemoveSwap();

    soft_target_mesh_vertical_radius_ =
        static_cast<float>(entity_overlay_settings_.soft_target.mesh->GetBounds().BoxExtent.Y);
    if (soft_target_mesh_vertical_radius_ <= UE_SMALL_NUMBER) {
        UE_LOG(LogSandboxUI,
               Error,
               TEXT("FHUDManager: World soft-target mesh has no vertical extent."));
        return;
    }
    instances->SetVisibility(true);
}

void FHUDManager::clear_world_soft_targets() {
    if (auto* const instances{soft_target_instances_.Get()}; IsValid(instances)) {
        instances->ClearInstances();
    }
}

void FHUDManager::add_world_soft_target(FRegistryEntityHandle const handle,
                                        float const range_alpha,
                                        float const indicator_radius_pixels,
                                        float const world_units_per_pixel,
                                        float const pulse,
                                        float const visibility,
                                        FVector const camera_location,
                                        FRotator const camera_rotation,
                                        FLinearColor const neutral_color,
                                        FLinearColor const in_range_color,
                                        FWorldSoftTargetCustomDataBuffer& custom_data) {
    auto* const instances{soft_target_instances_.Get()};
    if (!IsValid(instances) || !instances->IsVisible() || !handle.is_valid() ||
        !entity_registry->is_valid_alive(handle) || visibility <= 0.0f ||
        world_units_per_pixel <= UE_SMALL_NUMBER ||
        soft_target_mesh_vertical_radius_ <= UE_SMALL_NUMBER) {
        return;
    }

    auto const entities{entity_registry->get_entity_data().get_const_view()};
    auto const target_fit_radius{indicator_radius_pixels * world_units_per_pixel};
    auto const clamped_range_alpha{FMath::Clamp(range_alpha, 0.0f, 1.0f)};
    auto const closing_scale{FMath::Lerp(
        FMath::Max(entity_overlay_settings_.soft_target.bracket_start_radius_multiplier, 1.0f),
        1.0f,
        clamped_range_alpha)};
    auto const uniform_scale{target_fit_radius * closing_scale / soft_target_mesh_vertical_radius_};
    if (!FMath::IsFinite(uniform_scale) || uniform_scale <= UE_SMALL_NUMBER) {
        return;
    }

    FVector const location{entities.locations[handle.index]};
    auto const facing_direction{(camera_location - location).GetSafeNormal()};
    if (facing_direction.IsNearlyZero()) {
        return;
    }
    auto const camera_up{FRotationMatrix{camera_rotation}.GetUnitAxis(EAxis::Z)};
    auto const rotation{FRotationMatrix::MakeFromZY(facing_direction, camera_up).ToQuat()};
    FTransform const transform{rotation, location, FVector{uniform_scale}};
    auto const instance_index{instances->AddInstance(transform, true)};
    if (instance_index == INDEX_NONE) {
        UE_LOG(LogSandboxUI, Error, TEXT("FHUDManager: Failed to add world soft-target instance."));
        return;
    }

    auto const color{clamped_range_alpha >= 1.0f ? in_range_color : neutral_color};
    auto const opacity{
        FMath::Clamp((entity_overlay_settings_.soft_target.opacity +
                      pulse * entity_overlay_settings_.soft_target.pulse_opacity_boost) *
                         visibility * color.A,
                     0.0f,
                     1.0f)};
    check(custom_data.Num() == instance_index * ml::soft_target_world::custom_data_count);
    float instance_custom_data[ml::soft_target_world::custom_data_count]{};
    instance_custom_data[ml::soft_target_world::color_red_index] = color.R;
    instance_custom_data[ml::soft_target_world::color_green_index] = color.G;
    instance_custom_data[ml::soft_target_world::color_blue_index] = color.B;
    instance_custom_data[ml::soft_target_world::opacity_index] = opacity;
    instance_custom_data[ml::soft_target_world::intensity_index] =
        1.0f + FMath::Clamp(pulse, 0.0f, 1.0f);
    instance_custom_data[ml::soft_target_world::range_alpha_index] = clamped_range_alpha;
    custom_data.Append(instance_custom_data, ml::soft_target_world::custom_data_count);
}

void FHUDManager::update_radars() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FHUDManager::update_radars);
    TRACE_COUNTER_SET(SandboxRadarCandidateCount, 0);
    TRACE_COUNTER_SET(SandboxRadarVisibleCount, 0);
    TRACE_COUNTER_SET(SandboxRadarUploadBytes, 0);
    if (!radar_settings_.enabled || registered_huds.IsEmpty()) {
        return;
    }

    for (auto& registration : registered_huds) {
        update_radar(registration);
    }
}

void FHUDManager::update_radar(FRegisteredHud& registration) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Radar::Collect);
    auto* const hud{registration.ship_hud.Get()};
    if (!IsValid(hud)) {
        return;
    }
    if (!registration.radar_frame_store.IsValid()) {
        UE_LOG(LogSandboxUI, Error, TEXT("FHUDManager: Registered radar HUD is invalid."));
        return;
    }

    check(registration.radar_frame_store.IsValid());
    auto& frame{registration.radar_frame_store->next()};
    if (!validate_player_ship_for_collection() ||
        !entity_registry->is_valid_alive(player_ship->registry_handle)) {
        frame.instances.Reset();
        registration.radar_frame_store->publish();
        return;
    }

    check(entity_registry);
    auto const result{collect_radar_instances(entity_registry->get_entity_data().get_const_view(),
                                              entity_registry->get_generations(),
                                              entity_overlay_objective_roles_,
                                              radar_contact_colours_,
                                              player_ship->transform,
                                              player_ship->registry_handle,
                                              player_ship->lock_on_target,
                                              player_ship->team,
                                              radar_settings_,
                                              frame)};
    registration.radar_frame_store->publish();

    TRACE_COUNTER_SET(SandboxRadarCandidateCount, result.candidate_count);
    TRACE_COUNTER_SET(SandboxRadarVisibleCount, result.visible_count);
    TRACE_COUNTER_SET(SandboxRadarUploadBytes,
                      static_cast<int64>(result.visible_count) * sizeof(FRadarInstance));
}
bool FHUDManager::collect_mission_data() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FHUDManager::collect_mission_data);
    check(mission_manager);
    if (!mission_manager->is_ready()) {
        return false;
    }

    auto& next_data{mission_data_buffers.next()};
    read_mission_data(next_data);
    mission_data_buffers.cycle();
    has_mission_data = true;
    return mission_data_buffers.current() != mission_data_buffers.previous();
}
void FHUDManager::read_mission_data(ml::hud_manager::FMissionDataCache& out) const {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FHUDManager::read_mission_data);
    check(mission_manager);

    auto& static_data{out.static_data};
    static_data.mission_mode = mission_manager->get_mission_mode();
    static_data.surviving_entity_ids = mission_manager->get_entity_ids_that_must_survive();
    static_data.surviving_entity_types = mission_manager->get_entity_types_that_must_survive();
    static_data.required_kill_entity_ids = mission_manager->get_entity_ids_required_to_kill();
    static_data.required_kill_entity_types = mission_manager->get_entity_types_required_to_kill();

    auto& status_data{out.status_data};
    status_data.mission_state = mission_manager->get_mission_state();
    status_data.mission_stopwatch = mission_manager->get_mission_stopwatch();
    status_data.time_remaining = mission_manager->get_time_remaining();
    status_data.enemies_remaining = mission_manager->get_kills_remaining();
    status_data.surviving_entity_health = mission_manager->get_entity_health_that_must_survive();
    status_data.required_kill_entity_health = mission_manager->get_entity_health_required_to_kill();
}
bool FHUDManager::collect_entity_count_data() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FHUDManager::collect_entity_count_data);
    check(entity_registry);

    auto& next_data{entity_count_data_buffers.next()};
    next_data.alive_per_team_and_type = entity_registry->count_alive_per_team_and_type();
    entity_count_data_buffers.cycle();
    return entity_count_data_buffers.current() != entity_count_data_buffers.previous();
}
void FHUDManager::collect_kill_data() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FHUDManager::collect_kill_data);
    check(entity_registry);

    auto& next_data{kill_data_buffers.next()};
    auto const& unique_entities{entity_registry->get_unique_entities()};
    auto const n_unique_entities{unique_entities.num()};

    top_killer_ids_buffer.Reset();
    for (int32 entity_index{0}; entity_index < n_unique_entities; ++entity_index) {
        if (unique_entities.kills[entity_index] == 0) {
            continue;
        }
        top_killer_ids_buffer.Add({.id = entity_index});
    }
    Algo::Sort(top_killer_ids_buffer,
               [&unique_entities](TestEntityUniqueId const lhs, TestEntityUniqueId const rhs) {
                   auto const lhs_kills{unique_entities.kills[lhs.id]};
                   auto const rhs_kills{unique_entities.kills[rhs.id]};
                   return lhs_kills != rhs_kills ? lhs_kills > rhs_kills : lhs.id < rhs.id;
               });

    next_data.top_killers.reset();
    next_data.top_killers.add_defaulted(top_killer_ids_buffer.Num());
    auto const n_top_killers{top_killer_ids_buffer.Num()};
    for (int32 top_killer_index{0}; top_killer_index < n_top_killers; ++top_killer_index) {
        auto const entity_id{top_killer_ids_buffer[top_killer_index]};
        next_data.top_killers.entity_ids[top_killer_index] = entity_id;
        next_data.top_killers.entity_types[top_killer_index] =
            unique_entities.entity_types[entity_id.id];
        next_data.top_killers.teams[top_killer_index] = unique_entities.teams[entity_id.id];
        next_data.top_killers.kills[top_killer_index] =
            static_cast<int32>(unique_entities.kills[entity_id.id]);
    }

    next_data.team_kill_matrix = {};
    for (int32 victim_index{0}; victim_index < n_unique_entities; ++victim_index) {
        if (!unique_entities.killed_by[victim_index].is_valid()) {
            continue;
        }

        auto const killer_id{unique_entities.killed_by[victim_index]};
        check(killer_id.is_valid());
        auto const team_index{std::to_underlying(unique_entities.teams[killer_id.id])};
        auto const type_index{std::to_underlying(unique_entities.entity_types[victim_index])};
        if (team_index >= ml::ship_hud::FTeamKillMatrix::team_count ||
            type_index >= ml::ship_hud::FTeamKillMatrix::entity_type_count) {
            continue;
        }
        next_data.team_kill_matrix.add(unique_entities.teams[killer_id.id],
                                       unique_entities.entity_types[victim_index]);
    }
    kill_data_buffers.cycle();
}
bool FHUDManager::collect_player_status_data() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FHUDManager::collect_player_status_data);
    auto& next_data{player_status_data_buffers.next()};
    next_data = {};

    if (validate_player_ship_for_collection()) {
        next_data.has_player_ship = true;
        next_data.health = player_ship->health;
        next_data.speed = player_ship->get_speed();
        next_data.target_speed = player_ship->target_speed;
        next_data.energy = player_ship->get_energy();
        next_data.points = player_ship->get_kills();
        next_data.fire_rate = player_ship->laser_fire_rate;

        auto const firing_mode{player_ship->laser_firing_mode};
        if (firing_mode == ELaserFiringState::lock_on_searching ||
            firing_mode == ELaserFiringState::lock_on_acquired) {
            next_data.crosshair_targeting = true;
        }
    }

    player_status_data_buffers.cycle();
    return player_status_data_buffers.current() != player_status_data_buffers.previous();
}
bool FHUDManager::collect_player_flight_data() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FHUDManager::collect_player_flight_data);
    auto& next_data{player_flight_data_buffers.next()};
    next_data = {};
    next_data.selected_mapping_context = selected_mapping_context;

    if (validate_player_ship_for_collection()) {
        auto const ship_socket{player_ship->get_middle_socket()};
        auto const lock_on_target{player_ship->lock_on_target};

        next_data.has_player_ship = true;
        next_data.turning = player_ship->rotation_input;
        next_data.moving = player_ship->planar_movement_direction;
        next_data.desired_velocity_scale = player_ship->target_local_planar_velocity_scale;
        next_data.ship_velocity = player_ship->velocity;
        next_data.target_velocity = player_ship->target_local_planar_velocity;
        next_data.control_mode = player_ship->control_mode;
        next_data.flight_mode = player_ship->flight_mode;
        next_data.crosshair_origin = ship_socket.GetLocation();
        next_data.crosshair_direction = ship_socket.GetUnitAxis(EAxis::X);
        next_data.has_lock_on_target = entity_registry->is_valid_handle(lock_on_target);
        if (next_data.has_lock_on_target) {
            next_data.lock_on_target_position =
                FVector{entity_registry->get_location(lock_on_target)};
        }
    }

    player_flight_data_buffers.cycle();
    return player_flight_data_buffers.current() != player_flight_data_buffers.previous();
}
#if WITH_EDITOR
bool FHUDManager::collect_sampled_speed_data() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FHUDManager::collect_sampled_speed_data);
    auto& next_data{sampled_speed_data_buffers.next()};
    next_data = {};
    if (player_ship) {
        auto const& samples{player_ship->speed_samples};
        next_data.samples.Append(samples.GetData(), samples.Num());
        next_data.oldest_index = player_ship->speed_sample_index;
        has_sampled_speed_data = true;
    }

    sampled_speed_data_buffers.cycle();
    return sampled_speed_data_buffers.current() != sampled_speed_data_buffers.previous();
}
#endif

void FHUDManager::update_huds(ml::hud_manager::FDataChanges const& changes) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FHUDManager::update_huds);
    if (registered_huds.IsEmpty()) {
        return;
    }

    for (auto const& registration : registered_huds) {
        auto* const hud{registration.hud.Get()};
        check(IsValid(hud));

        if (changes.mission) {
            update_mission_hud(*hud);
        }
        if (changes.entity_counts) {
            update_entity_count_hud(*hud);
        }
        auto* const ship_hud{registration.ship_hud.Get()};
        if (IsValid(ship_hud) && changes.player_status) {
            update_player_status_hud(*ship_hud);
        }
        if (IsValid(ship_hud) && changes.player_flight) {
            update_player_flight_hud(*ship_hud);
        }
#if WITH_EDITOR
        if (IsValid(ship_hud) && changes.sampled_speed) {
            update_sampled_speed_hud(*ship_hud);
        }
#endif
    }
}
void FHUDManager::synchronise_hud(USimulationHudWidget& hud) const {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FHUDManager::synchronise_hud);
    check(IsValid(&hud));

    if (has_mission_data) {
        update_mission_hud(hud);
    }
    update_entity_count_hud(hud);
    auto* const ship_hud{Cast<UShipHudWidget>(&hud)};
    if (!IsValid(ship_hud)) {
        return;
    }
    update_player_status_hud(*ship_hud);
    update_player_flight_hud(*ship_hud);
#if WITH_EDITOR
    if (has_sampled_speed_data) {
        update_sampled_speed_hud(*ship_hud);
    }
#endif
}

void FHUDManager::update_mission_hud(USimulationHudWidget& hud) const {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FHUDManager::update_mission_hud);
    check(has_mission_data);
    auto const& data{mission_data_buffers.current()};
    hud.set_mission_data(data);
    if (auto* const ship_hud{Cast<UShipHudWidget>(&hud)}; IsValid(ship_hud)) {
        ship_hud->set_stopwatch_time(data.status_data.mission_stopwatch);
    }
}
void FHUDManager::update_entity_count_hud(USimulationHudWidget& hud) const {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FHUDManager::update_entity_count_hud);
    hud.set_entity_counts(entity_count_data_buffers.current().alive_per_team_and_type);
}
void FHUDManager::update_player_status_hud(UShipHudWidget& hud) const {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FHUDManager::update_player_status_hud);
    auto const& data{player_status_data_buffers.current()};
    if (!data.has_player_ship) {
        return;
    }

    hud.set_health(data.health);
    hud.set_speed(data.speed);
    hud.set_target_speed(data.target_speed);
    hud.set_energy(data.energy);
    hud.set_points(data.points);
    hud.set_fire_rate(*ml::to_string_without_type_prefix(data.fire_rate));
    hud.set_crosshair_targeting(data.crosshair_targeting);
}
void FHUDManager::update_player_flight_hud(UShipHudWidget& hud) const {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FHUDManager::update_player_flight_hud);
    auto const& data{player_flight_data_buffers.current()};
    if (!data.has_player_ship) {
        hud.set_crosshair_widget_visibility(ESlateVisibility::Collapsed);
        hud.set_lock_on_widget_visibility(false);
        return;
    }

    hud.set_crosshair_widget_visibility(ESlateVisibility::Visible);
    hud.set_lock_on_widget_visibility(data.has_lock_on_target);
    hud.set_selected_imc(FStringView{data.selected_mapping_context});
    hud.set_turning(data.turning);
    hud.set_moving(data.moving);
    hud.set_desired_velocity_scale(data.desired_velocity_scale);
    hud.set_ship_velocity(data.ship_velocity);
    hud.set_target_velocity(data.target_velocity);
    hud.set_control_mode(*ml::to_string_without_type_prefix(data.control_mode));
    hud.set_flight_mode(*ml::to_string_without_type_prefix(data.flight_mode));

    auto* const controller{hud.GetOwningPlayer()};
    check(IsValid(controller));

    auto const& distances{hud.get_crosshair_distances()};
    auto const near_world_position{data.crosshair_origin +
                                   data.crosshair_direction * distances.near};
    auto const far_world_position{data.crosshair_origin + data.crosshair_direction * distances.far};
    FVector2d near_screen_position{};
    FVector2d far_screen_position{};
    constexpr bool player_viewport_relative{false};
    if (!UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(
            controller, near_world_position, near_screen_position, player_viewport_relative)) {
        UE_LOG(
            LogSandboxUI, Warning, TEXT("FHUDManager: Failed to project near crosshair position."));
    }
    if (!UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(
            controller, far_world_position, far_screen_position, player_viewport_relative)) {
        UE_LOG(
            LogSandboxUI, Warning, TEXT("FHUDManager: Failed to project far crosshair position."));
    }
    hud.set_crosshair_positions(near_screen_position, far_screen_position);

    if (data.has_lock_on_target) {
        FVector2d lock_on_screen_position{};
        if (!UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(
                controller,
                data.lock_on_target_position,
                lock_on_screen_position,
                player_viewport_relative)) {
            UE_LOG(LogSandboxUI, Warning, TEXT("FHUDManager: Failed to project lock-on position."));
        }
        hud.set_lock_on_widget_position(lock_on_screen_position);
    }
}
#if WITH_EDITOR
void FHUDManager::update_sampled_speed_hud(UShipHudWidget& hud) const {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FHUDManager::update_sampled_speed_hud);
    check(has_sampled_speed_data);
    auto const& data{sampled_speed_data_buffers.current()};
    hud.update_sampled_speed(TConstArrayView<FVector2d>{data.samples}, data.oldest_index);
}
#endif

bool FHUDManager::validate_player_ship_for_collection() const {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FHUDManager::validate_player_ship_for_collection);
    if (!player_ship) {
        return false;
    }

    check(entity_registry);
    auto const unique_id{player_ship->unique_entity_id};
    return unique_id.is_valid() && entity_registry->is_valid_unique_id(unique_id);
}
