#pragma once

#include <ioj/periodic_tick_countdown.h>
#include <ioj/presentation/entity_overlay.h>
#include <ioj/presentation/hud_model.h>
#include <ioj/presentation/radar.h>
#include <ioj/presentation/soft_target.h>
#include <ioj/settings/entity_overlay.h>
#include <SpaceGamePresentation/presentation/EntityOverlaySource.h>
#include <SpaceGamePresentation/presentation/LevelPresentationSettings.h>
#include <SpaceGamePresentation/presentation/WorldSoftTargetMaterialData.h>

#include <SandboxUI/EntityOverlay/EntityOverlayFrameStore.h>
#include <SandboxUI/EntityOverlay/EntityOverlayTypes.h>
#include <SandboxUI/Radar/RadarFrameStore.h>
#include <SandboxUI/Radar/RadarTypes.h>

#include <CoreMinimal.h>
#include <HAL/Platform.h>
#include <UObject/WeakObjectPtrTemplates.h>

struct FLevelVisualConfig;
class USimulationHudWidget;
class UShipHudWidget;
class UInstancedStaticMeshComponent;

namespace ioj::sim {
class Mission;
struct SpatialQueryManager;
}

namespace ioj {
enum class EHUDManagerState : uint8 {
    Disabled,
    Active,
};

inline auto LexToString(EHUDManagerState const value) -> FString {
    switch (value) {
        case EHUDManagerState::Active: {
            return TEXT("Active");
        }
        case EHUDManagerState::Disabled: {
            return TEXT("Disabled");
        }
        default:
            break;
    }
    return TEXT("???");
}

struct SPACEGAMEPRESENTATION_API FHUDManager {
    void configure(sim::HudSettings settings) { model_.configure(settings); }
    auto settings() const -> sim::HudSettings const& { return model_.settings(); }

    void initialise(sim::Mission const& new_mission,
                    sim::LevelReadAccess view,
                    sim::SpatialQueryManager const& new_spatial_query_manager,
                    FLevelVisualConfig const& level_config,
                    UInstancedStaticMeshComponent* soft_target_instances = nullptr);
    void deactivate();
    void advance(double dt);
    void tick(PeriodicTickCountdown8::counter_type num_ticks);
    void force_sample();

    void register_hud(USimulationHudWidget& hud);
    void unregister_hud(USimulationHudWidget& hud);
    void set_selected_mapping_context(FString const& context_name);

    auto get_state() const noexcept -> EHUDManagerState {
        return model_.active() ? EHUDManagerState::Active : EHUDManagerState::Disabled;
    }
    auto get_registered_hud_count() const noexcept -> int32 { return registered_huds.Num(); }
    auto get_mission_data() const -> presentation::MissionHudData const& {
        return model_.mission_data();
    }
    auto get_entity_count_data() const -> presentation::EntityCountHudData const& {
        return model_.entity_count_data();
    }
    auto get_kill_data() const -> presentation::KillHudData const& { return model_.kill_data(); }
    auto get_player_status_data() const -> presentation::PlayerStatusHudData const& {
        return model_.player_status_data();
    }
    auto get_player_flight_data() const -> presentation::PlayerFlightHudData const& {
        return model_.player_flight_data();
    }
  private:
    using FWorldSoftTargetCustomDataBuffer =
        TArray<float, TInlineAllocator<soft_target_world::custom_data_count * 2>>;

    struct FRegisteredHud {
        TWeakObjectPtr<USimulationHudWidget> hud;
        TWeakObjectPtr<UShipHudWidget> ship_hud;
        FEntityOverlayFrameStorePtr entity_overlay_frame_store;
        std::vector<ui::entity_overlay::Instance> overlay_instances;
        presentation::RadarFrame radar_frame;
        FRadarFrameStorePtr radar_frame_store;
        presentation::SoftTargetState soft_targets{};
    };

    void update_entity_overlays(float delta_seconds);
    void update_entity_overlay_objective_roles();
    void update_entity_overlay(FRegisteredHud& registration,
                               float delta_seconds,
                               bool render_world_target);
    void configure_world_soft_target_renderer();
    void clear_world_soft_targets();
    void add_world_soft_target(presentation::SoftTargetMarker const& target,
                               FVector camera_location,
                               FRotator camera_rotation,
                               FLinearColor neutral_color,
                               FLinearColor in_range_color,
                               FWorldSoftTargetCustomDataBuffer& custom_data);
    void update_radars();
    void update_radar(FRegisteredHud& registration);

    template <typename Hud>
    void update_hud(Hud& hud, presentation::HudChanges const& changes);
    void update_huds(presentation::HudChanges const& changes);
    void synchronise_hud(USimulationHudWidget& hud) const;
    void update_mission_hud(USimulationHudWidget& hud) const;
    void update_entity_count_hud(USimulationHudWidget& hud) const;
    void update_player_status_hud(UShipHudWidget& hud) const;
    void update_player_flight_hud(UShipHudWidget& hud) const;
#if WITH_EDITOR
    void update_sampled_speed_hud(UShipHudWidget& hud) const;
#endif

    presentation::HudModel model_;
    TArray<FRegisteredHud> registered_huds;
    sim::SpatialQueryManager const* spatial_query_manager{};
    FEntityOverlaySettings entity_overlay_resources_;
    ioj::settings::EntityOverlay entity_overlay_settings_;
    FEntityOverlayStyle entity_overlay_style_;
    presentation::EntityOverlayTeamColours entity_overlay_team_colours_;
    presentation::EntityOverlayHealthMaximums entity_overlay_maximum_health_;
    TArray<ui::entity_overlay::ObjectiveRole> entity_overlay_objective_roles_;
    ioj::settings::SoftTarget soft_target_selection_settings_;
    TWeakObjectPtr<UInstancedStaticMeshComponent> soft_target_instances_;
    float soft_target_mesh_vertical_radius_{0.0f};
    bool warned_multiple_world_target_views_{false};
    ioj::settings::Radar radar_settings_;
    FRadarStyle radar_style_;
    presentation::RadarContactColours radar_contact_colours_;
};

}
