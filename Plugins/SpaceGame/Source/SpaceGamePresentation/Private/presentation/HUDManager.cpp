#include "SpaceGamePresentation/presentation/HUDManager.h"

#include "ioj/sim/collision/spatial_query_manager.h"
#include "ioj/sim/player/sim.h"
#include "SpaceGamePresentation/entities/TestTeamVisualData.h"
#include "SpaceGamePresentation/presentation/widgets/ShipHudWidget.h"
#include "SpaceGamePresentation/presentation/widgets/SimulationHudWidget.h"
#include <ioj/sim/entities/entity_type.h>
#include <ioj/team_array.h>
#include <SpaceGamePresentation/entities/TestTeamConversion.h>
#include <SpaceGamePresentation/presentation/LevelActorSettings.h>
#include <SpaceGamePresentation/support/logging/PresentationLogCategories.h>

#include <SandboxCoreEngine/strings.h>
#include <SandboxCoreEngine/VectorConversion.h>

#include <Blueprint/WidgetLayoutLibrary.h>
#include <Components/InstancedStaticMeshComponent.h>
#include <Engine/StaticMesh.h>
#include <GameFramework/PlayerController.h>
#include <Materials/MaterialInterface.h>
#include <ProfilingDebugging/CountersTrace.h>

#include <concepts>
#include <span>

TRACE_DECLARE_INT_COUNTER(SandboxEntityOverlayCandidateCount,
                          TEXT("Sandbox/EntityOverlay/CandidateCount"));
TRACE_DECLARE_INT_COUNTER(SandboxEntityOverlayInvalidHealthCount,
                          TEXT("Sandbox/EntityOverlay/InvalidHealthCount"));
TRACE_DECLARE_INT_COUNTER(SandboxEntityOverlayUploadBytes,
                          TEXT("Sandbox/EntityOverlay/UploadBytes"));
TRACE_DECLARE_INT_COUNTER(SandboxRadarCandidateCount, TEXT("Sandbox/Radar/CandidateCount"));
TRACE_DECLARE_INT_COUNTER(SandboxRadarVisibleCount, TEXT("Sandbox/Radar/VisibleCount"));
TRACE_DECLARE_INT_COUNTER(SandboxRadarUploadBytes, TEXT("Sandbox/Radar/UploadBytes"));

namespace ioj {
void FHUDManager::initialise(sim::LevelReadAccess const view,
                             sim::SpatialQueryManager const& new_spatial_query_manager,
                             FLevelVisualConfig const& level_config,
                             UInstancedStaticMeshComponent* const soft_target_instances) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FHUDManager::initialise);
    model_.initialise(view);
    spatial_query_manager = &new_spatial_query_manager;
    auto const& entity_overlay_settings{level_config.entity_overlay};
    entity_overlay_resources_ = entity_overlay_settings;
    entity_overlay_settings_ =
        presentation::sanitize_entity_overlay_settings(level_config.presentation.entity_overlay);
    auto const& overlay{entity_overlay_settings_};
    soft_target_instances_ = soft_target_instances;
    warned_multiple_world_target_views_ = false;
    configure_world_soft_target_renderer();
    radar_settings_ = ioj::presentation::sanitize_radar_settings(level_config.presentation.radar);
    radar_style_.grid_opacity = radar_settings_.grid_opacity;
    radar_style_.core_cell_radius = radar_settings_.core_cell_radius;
    radar_style_.combat_cell_radius = radar_settings_.combat_cell_radius;
    radar_style_.tactical_cell_radius = radar_settings_.tactical_cell_radius;
    radar_style_.strategic_cell_radius = radar_settings_.strategic_cell_radius;
    radar_style_.glyph_size_scale = radar_settings_.glyph_size_scale;
    radar_style_.objective_size_multiplier = radar_settings_.objective_size_multiplier;
    radar_style_.objective_ring_padding_pixels = radar_settings_.objective_ring_padding_pixels;
    radar_style_.objective_ring_thickness_pixels = radar_settings_.objective_ring_thickness_pixels;
    auto const& spatial_queries{new_spatial_query_manager};
    auto const fighter_radius{spatial_queries.entity_type_radius(sim::EntityType::Fighter)};
    auto const capital_radius{spatial_queries.entity_type_radius(sim::EntityType::CapitalShip)};
    entity_overlay_style_ = {
        .bar_size_pixels = FVector2f{ioj::to_unreal(overlay.bar_size_pixels)},
        .screen_offset_pixels = FVector2f{ioj::to_unreal(overlay.screen_offset_pixels)},
        .minimum_world_radius = FMath::Min(fighter_radius, capital_radius),
        .maximum_world_radius = FMath::Max(fighter_radius, capital_radius),
        .minimum_bar_scale = overlay.minimum_bar_scale,
        .maximum_bar_scale = overlay.maximum_bar_scale,
        .inset_pixels = overlay.inset_pixels,
        .maximum_inset_height_ratio = overlay.maximum_inset_height_ratio,
        .objective_bar_height_scale = overlay.objective_bar_height_scale,
        .objective_frame_pixels = overlay.objective_frame_pixels,
        .screen_edge_padding_pixels = overlay.screen_edge_padding_pixels,
        .background_color = entity_overlay_settings.background_color,
        .fill_color = entity_overlay_settings.fill_color,
    };
    auto native_colours = [](UTestTeamVisualData const* data) {
        auto const colours{UTestTeamVisualData::build_team_colour_cache(data)};
        TeamArray<ui::Color4f> result;
        constexpr auto count{enum_count<Team>()};
        for (std::size_t index{}; index < count; ++index) {
            auto const colour{colours[static_cast<ETestTeam>(index)]};
            result[static_cast<Team>(index)] = {colour.R, colour.G, colour.B, colour.A};
        }
        return result;
    };
    entity_overlay_team_colours_ = {
        .capital_ship = native_colours(level_config.capital_ships.team_visual_data),
        .fighter = native_colours(level_config.fighters.team_visual_data),
        .turret = native_colours(level_config.turrets.team_visual_data)};
    entity_overlay_maximum_health_ = {
        .capital_ship = level_config.capital_ships.simulation.max_health.get().value(),
        .fighter = level_config.fighters.simulation.health.get().value(),
        .turret = level_config.turrets.simulation.max_health.get().value(),
    };
    soft_target_selection_settings_ = overlay.soft_target;
    entity_overlay_objective_roles_.Reset();

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
        registration.soft_targets = {};
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
    model_.deactivate();
    spatial_query_manager = nullptr;
    entity_overlay_objective_roles_.Reset();
    soft_target_instances_ = nullptr;
    soft_target_mesh_vertical_radius_ = 0.0f;
    warned_multiple_world_target_views_ = false;
}

void FHUDManager::advance(double const dt) {
    if (!model_.active()) {
        return;
    }
    update_huds(model_.advance(dt));
    update_entity_overlays(static_cast<float>(dt));
    update_radars();
}
void FHUDManager::tick(PeriodicTickCountdown8::counter_type const num_ticks) {
    if (!model_.active()) {
        return;
    }
    update_huds(model_.tick(num_ticks));
    update_entity_overlays(static_cast<float>(num_ticks) * model_.seconds_per_tick());
    update_radars();
}
void FHUDManager::force_sample() {
    if (!model_.active()) {
        return;
    }
    update_huds(model_.force_sample());
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
    if (model_.active() && entity_overlay_settings_.enabled) {
        hud.set_entity_overlay_frame_store(registration.entity_overlay_frame_store);
    } else {
        hud.set_entity_overlay_frame_store({});
    }
    if (auto* const ship_hud{registration.ship_hud.Get()}; IsValid(ship_hud)) {
        registration.radar_frame_store = MakeShared<FRadarFrameStore, ESPMode::ThreadSafe>();
        ship_hud->set_radar_style(radar_style_);
        ship_hud->set_radar_frame_store(model_.active() && radar_settings_.enabled
                                            ? registration.radar_frame_store
                                            : FRadarFrameStorePtr{});
    }
    if (model_.active()) {
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
    model_.set_selected_mapping_context(TCHAR_TO_UTF8(*context_name));
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
    // Generic expressions do not imply a unique defend/destroy presentation role.
    entity_overlay_objective_roles_.Init(
        ui::entity_overlay::ObjectiveRole::None,
        presentation::display_entity_count(model_.display_batches()));
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
    FEntityOverlayView overlay_view;
    if (!hud->try_get_entity_overlay_view(overlay_view)) {
        UE_LOG(LogSandboxUI, Warning, TEXT("FHUDManager: Entity overlay view is invalid."));
        frame.instances.Reset();
        registration.soft_targets = {};
        registration.entity_overlay_frame_store->publish();
        return;
    }

    FVector camera_location{};
    FRotator camera_rotation{};
    auto* const controller{hud->GetOwningPlayer()};
    if (!IsValid(controller)) {
        UE_LOG(LogSandboxUI, Error, TEXT("FHUDManager: Entity overlay has no player controller."));
        frame.instances.Reset();
        registration.soft_targets = {};
        registration.entity_overlay_frame_store->publish();
        return;
    }
    controller->GetPlayerViewPoint(camera_location, camera_rotation);

    auto const batches{model_.display_batches()};
    auto const& entity_radii{spatial_query_manager->entity_type_radii()};
    ioj::presentation::SoftTargetResult soft_target;
    if (auto const* player_ship{registration.ship_hud.IsValid() ? model_.player_for_collection()
                                                                : nullptr}) {
        auto const firing_transform{player_ship->get_middle_socket()};
        auto const camera_transform{FRotationMatrix{camera_rotation}};
        soft_target =
            select_soft_target(batches,
                               entity_radii,
                               {entity_overlay_objective_roles_.GetData(),
                                static_cast<std::size_t>(entity_overlay_objective_roles_.Num())},
                               {.view = overlay_view,
                                .aim_origin = FVector3f{to_unreal(firing_transform.location)},
                                .aim_direction = FVector3f{to_unreal(firing_transform.forward())},
                                .camera_right = FVector3f{camera_transform.GetUnitAxis(EAxis::Y)},
                                .camera_up = FVector3f{camera_transform.GetUnitAxis(EAxis::Z)},
                                .player_team = to_unreal(player_ship->team),
                                .effective_weapon_range = player_ship->laser_effective_range(),
                                .maximum_overlay_range = entity_overlay_settings_.maximum_range},
                               soft_target_selection_settings_,
                               registration.soft_targets.current_target());
    }

    auto const markers{registration.soft_targets.advance(
        batches,
        soft_target,
        {ioj::NonNegativeFinite<float>::make(delta_seconds > 0.f ? delta_seconds : 0.f),
         soft_target_selection_settings_.pulse_duration,
         soft_target_selection_settings_.fade_out_duration})};

    if (render_world_target) {
        auto const neutral_color{hud->get_soft_target_neutral_colour()};
        auto const in_range_color{hud->get_soft_target_in_range_colour()};
        FWorldSoftTargetCustomDataBuffer custom_data;
        for (auto const& marker : markers) {
            add_world_soft_target(marker,
                                  camera_location,
                                  camera_rotation,
                                  neutral_color,
                                  in_range_color,
                                  custom_data);
        }

        if (!custom_data.IsEmpty()) {
            auto* const instances{soft_target_instances_.Get()};
            auto const instance_count{custom_data.Num() / soft_target_world::custom_data_count};
            check(IsValid(instances));
            check(custom_data.Num() == instance_count * soft_target_world::custom_data_count);
            check(instance_count == instances->GetInstanceCount());
            if (!instances->SetCustomData(0, instance_count - 1, custom_data, true)) {
                UE_LOG(LogSandboxUI,
                       Error,
                       TEXT("FHUDManager: Failed to upload world soft-target custom data."));
            }
        }
    }

    auto const result{presentation::collect_entity_overlay_instances(
        batches,
        entity_radii,
        std::span{entity_overlay_objective_roles_.GetData(),
                  static_cast<std::size_t>(entity_overlay_objective_roles_.Num())},
        entity_overlay_team_colours_,
        entity_overlay_maximum_health_,
        Vector3f{overlay_view.camera_origin},
        entity_overlay_settings_.maximum_range,
        registration.overlay_instances)};
    frame.instances.Reset();
    frame.instances.Append(registration.overlay_instances.data(),
                           static_cast<int32>(registration.overlay_instances.size()));
    registration.entity_overlay_frame_store->publish();

    TRACE_COUNTER_SET(SandboxEntityOverlayCandidateCount, result.candidate_count);
    TRACE_COUNTER_SET(SandboxEntityOverlayInvalidHealthCount, result.invalid_health_count);
    TRACE_COUNTER_SET(SandboxEntityOverlayUploadBytes,
                      static_cast<int64>(registration.overlay_instances.size()) *
                          sizeof(ui::entity_overlay::Instance));
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
    if (!IsValid(entity_overlay_resources_.soft_target.mesh) ||
        !IsValid(entity_overlay_resources_.soft_target.material)) {
        UE_LOG(LogSandboxUI,
               Error,
               TEXT("FHUDManager: World soft-target mesh or material is invalid."));
        return;
    }

    instances->SetMobility(EComponentMobility::Movable);
    instances->SetStaticMesh(entity_overlay_resources_.soft_target.mesh);
    auto const material_count{instances->GetNumMaterials()};
    for (int32 material_index{0}; material_index < material_count; ++material_index) {
        instances->SetMaterial(material_index, entity_overlay_resources_.soft_target.material);
    }
    instances->SetNumCustomDataFloats(soft_target_world::custom_data_count);
    instances->SetCanEverAffectNavigation(false);
    instances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    instances->SetGenerateOverlapEvents(false);
    instances->SetCastShadow(false);
    instances->SetAffectDistanceFieldLighting(false);
    instances->SetReceivesDecals(false);
    instances->SetRemoveSwap();

    soft_target_mesh_vertical_radius_ =
        static_cast<float>(entity_overlay_resources_.soft_target.mesh->GetBounds().BoxExtent.Y);
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

void FHUDManager::add_world_soft_target(presentation::SoftTargetMarker const& target,
                                        FVector const camera_location,
                                        FRotator const camera_rotation,
                                        FLinearColor const neutral_color,
                                        FLinearColor const in_range_color,
                                        FWorldSoftTargetCustomDataBuffer& custom_data) {
    auto* const instances{soft_target_instances_.Get()};
    if (!IsValid(instances) || !instances->IsVisible()) {
        return;
    }

    auto const visual{presentation::make_world_soft_target_visual(
        target,
        entity_overlay_settings_.soft_target,
        soft_target_mesh_vertical_radius_,
        {neutral_color.R, neutral_color.G, neutral_color.B, neutral_color.A},
        {in_range_color.R, in_range_color.G, in_range_color.B, in_range_color.A})};
    if (!visual) {
        return;
    }

    FVector const location{to_unreal(*target.location)};
    auto const facing_direction{(camera_location - location).GetSafeNormal()};
    if (facing_direction.IsNearlyZero()) {
        return;
    }
    auto const camera_up{FRotationMatrix{camera_rotation}.GetUnitAxis(EAxis::Z)};
    auto const rotation{FRotationMatrix::MakeFromZY(facing_direction, camera_up).ToQuat()};
    FTransform const transform{rotation, location, FVector{visual->scale}};
    auto const instance_index{instances->AddInstance(transform, true)};
    if (instance_index == INDEX_NONE) {
        UE_LOG(LogSandboxUI, Error, TEXT("FHUDManager: Failed to add world soft-target instance."));
        return;
    }

    check(custom_data.Num() == instance_index * soft_target_world::custom_data_count);
    float instance_custom_data[soft_target_world::custom_data_count]{};
    instance_custom_data[soft_target_world::color_red_index] = visual->colour.r;
    instance_custom_data[soft_target_world::color_green_index] = visual->colour.g;
    instance_custom_data[soft_target_world::color_blue_index] = visual->colour.b;
    instance_custom_data[soft_target_world::opacity_index] = visual->colour.a;
    instance_custom_data[soft_target_world::pulse_index] = visual->pulse;
    instance_custom_data[soft_target_world::range_alpha_index] = visual->range_alpha;
    custom_data.Append(instance_custom_data, soft_target_world::custom_data_count);
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
    auto const* player_ship{model_.player_for_collection()};
    if (!player_ship || !player_ship->is_alive()) {
        frame.instances.Reset();
        registration.radar_frame_store->publish();
        return;
    }

    auto const result{presentation::collect_radar_instances(
        model_.display_batches(),
        std::span{entity_overlay_objective_roles_.GetData(),
                  static_cast<std::size_t>(entity_overlay_objective_roles_.Num())},
        radar_contact_colours_,
        player_ship->physical_state().transform,
        player_ship->unique_entity_id,
        player_ship->team,
        radar_settings_,
        registration.radar_frame)};
    frame.instances.Reset();
    frame.instances.Append(registration.radar_frame.instances.data(),
                           static_cast<int32>(registration.radar_frame.instances.size()));
    frame.combat_display_radius = registration.radar_frame.combat_display_radius;
    frame.tactical_display_radius = registration.radar_frame.tactical_display_radius;
    registration.radar_frame_store->publish();

    TRACE_COUNTER_SET(SandboxRadarCandidateCount, result.candidate_count);
    TRACE_COUNTER_SET(SandboxRadarVisibleCount, result.visible_count);
    TRACE_COUNTER_SET(SandboxRadarUploadBytes,
                      static_cast<int64>(result.visible_count) * sizeof(ui::radar::Instance));
}
template <typename Hud>
void FHUDManager::update_hud(Hud& hud, presentation::HudChanges const& changes) {
    if (changes.entity_counts) {
        update_entity_count_hud(hud);
    }
    if constexpr (std::same_as<Hud, UShipHudWidget>) {
        if (changes.player_status) {
            update_player_status_hud(hud);
        }
        if (changes.player_flight) {
            update_player_flight_hud(hud);
        }
#if WITH_EDITOR
        if (changes.sampled_speed) {
            update_sampled_speed_hud(hud);
        }
#endif
    }
}
void FHUDManager::update_huds(presentation::HudChanges const& changes) {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FHUDManager::update_huds);
    for (auto const& registration : registered_huds) {
        auto* const hud{registration.hud.Get()};
        check(IsValid(hud));
        if (auto* const ship_hud{registration.ship_hud.Get()}) {
            update_hud(*ship_hud, changes);
        } else {
            update_hud(*hud, changes);
        }
    }
}
void FHUDManager::synchronise_hud(USimulationHudWidget& hud) const {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FHUDManager::synchronise_hud);
    check(IsValid(&hud));

    update_entity_count_hud(hud);
    auto* const ship_hud{Cast<UShipHudWidget>(&hud)};
    if (!IsValid(ship_hud)) {
        return;
    }
    update_player_status_hud(*ship_hud);
    update_player_flight_hud(*ship_hud);
#if WITH_EDITOR
    update_sampled_speed_hud(*ship_hud);
#endif
}

void FHUDManager::update_entity_count_hud(USimulationHudWidget& hud) const {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FHUDManager::update_entity_count_hud);
    hud.set_entity_counts(model_.entity_count_data().alive_per_team_and_type);
}
void FHUDManager::update_player_status_hud(UShipHudWidget& hud) const {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FHUDManager::update_player_status_hud);
    auto const& data{model_.player_status_data()};
    if (!data) {
        return;
    }

    hud.set_health({data->health.health.value(), data->health.max_health.value()});
    hud.set_speed(data->speed);
    hud.set_target_speed(data->target_speed);
    hud.set_energy(data->energy);
    hud.set_points(data->points);
    hud.set_fire_rate(to_fstring(sim::to_string(data->fire_rate)));
}
void FHUDManager::update_player_flight_hud(UShipHudWidget& hud) const {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FHUDManager::update_player_flight_hud);
    auto const& data{model_.player_flight_data()};
    if (!data) {
        hud.set_crosshair_widget_visibility(ESlateVisibility::Collapsed);
        return;
    }

    hud.set_crosshair_widget_visibility(ESlateVisibility::Visible);
    hud.set_selected_imc(to_fstring(data->selected_mapping_context));
    hud.set_flight_vector_debug(data->flight_vector_debug);
    hud.set_ship_velocity(to_unreal(data->ship_velocity));
    hud.set_target_velocity(to_unreal(data->target_velocity));
    FString flight_model;
    switch (data->flight_model_preset) {
        case sim::player::FlightModelPreset::Starfox:
            flight_model = TEXT("Starfox");
            break;
        case sim::player::FlightModelPreset::Fighter:
            flight_model = TEXT("Fighter");
            break;
        case sim::player::FlightModelPreset::Skater:
            flight_model = TEXT("Skater");
            break;
        case sim::player::FlightModelPreset::Gunship:
            flight_model = TEXT("Gunship");
            break;
    }
    if (data->flight_model_customized) {
        flight_model = FString::Printf(TEXT("Custom (based on %s)"), *flight_model);
    }
    hud.set_flight_model(flight_model);
    hud.set_flight_action(data->throttle,
                          to_fstring(sim::player::to_string(data->boost_brake_state)));

    auto* const controller{hud.GetOwningPlayer()};
    check(IsValid(controller));

    auto const& distances{hud.get_crosshair_distances()};
    auto const near_world_position{data->crosshair_origin +
                                   data->crosshair_direction * distances.near};
    auto const far_world_position{data->crosshair_origin +
                                  data->crosshair_direction * distances.far};
    FVector2d near_screen_position{};
    FVector2d far_screen_position{};
    constexpr bool player_viewport_relative{false};
    if (!UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(controller,
                                                                    to_unreal(near_world_position),
                                                                    near_screen_position,
                                                                    player_viewport_relative)) {
        UE_LOG(
            LogSandboxUI, Warning, TEXT("FHUDManager: Failed to project near crosshair position."));
    }
    if (!UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(controller,
                                                                    to_unreal(far_world_position),
                                                                    far_screen_position,
                                                                    player_viewport_relative)) {
        UE_LOG(
            LogSandboxUI, Warning, TEXT("FHUDManager: Failed to project far crosshair position."));
    }
    hud.set_crosshair_positions(near_screen_position, far_screen_position);
}
#if WITH_EDITOR
void FHUDManager::update_sampled_speed_hud(UShipHudWidget& hud) const {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FHUDManager::update_sampled_speed_hud);
    if (auto const data{model_.sampled_speed_view()}) {
        hud.update_sampled_speed(data->samples, data->oldest_index);
    }
}
#endif

}
