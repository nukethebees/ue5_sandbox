#pragma once

#include <ioj/multi_buffer.h>
#include <ioj/periodic_tick_countdown.h>
#include <ioj/presentation/soft_target.h>
#include <ioj/settings/entity_overlay.h>
#include <ioj/sim/entities/entity_ledger.h>
#include <ioj/sim/entities/entity_type.h>
#include <ioj/sim/fixed_tick_loop.h>
#include <ioj/sim/levels/simulation_settings.h>
#include <ioj/sim/player/fire_rate.h>
#include <ioj/sim/player/flight_model_config.h>
#include <ioj/sim/player/space_ship_common.h>
#include <SpaceGamePresentation/entities/ShipHealth.h>
#include <SpaceGamePresentation/presentation/EntityOverlaySource.h>
#include <SpaceGamePresentation/presentation/LevelPresentationSettings.h>
#include <SpaceGamePresentation/presentation/RadarSource.h>
#include <SpaceGamePresentation/presentation/widgets/FlightVectorDebugData.h>
#include <SpaceGamePresentation/presentation/widgets/ShipHudKillData.h>
#include <SpaceGamePresentation/presentation/WorldSoftTargetMaterialData.h>

#include <SandboxUI/EntityOverlay/EntityOverlayFrameStore.h>
#include <SandboxUI/EntityOverlay/EntityOverlayTypes.h>
#include <SandboxUI/Radar/RadarFrameStore.h>
#include <SandboxUI/Radar/RadarTypes.h>

#include <CoreMinimal.h>
#include <HAL/Platform.h>
#include <UObject/WeakObjectPtrTemplates.h>

#include <array>

struct FLevelVisualConfig;
class USimulationHudWidget;
class UShipHudWidget;
class UInstancedStaticMeshComponent;

namespace ioj::sim {
class Mission;
struct SpatialQueryManager;
}
namespace ioj::sim::player {
struct Sim;
}
namespace ioj::sim::capital_ships {
struct Sim;
}
namespace ioj::sim::fighters {
struct Sim;
}
namespace ioj::sim::turrets {
struct Sim;
}
namespace ioj::sim::spinners {
struct Sim;
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

struct FHUDUpdateTimerIndex {
    static constexpr int32 player_status{0};
    static constexpr int32 entity_counts{1};
    static constexpr int32 mission_status{2};
};

namespace hud_manager {
struct FMissionDataCache {
    bool operator==(FMissionDataCache const&) const noexcept = default;
    FString active_titles;
    int32 active{};
    int32 completed{};
    int32 inactive{};
};

struct FEntityCountDataCache {
    bool operator==(FEntityCountDataCache const& other) const noexcept = default;

    sim::telemetry::EntityCounts alive_per_team_and_type{};
};

struct FKillDataCache {
    bool operator==(FKillDataCache const& other) const noexcept = default;

    TArray<ship_hud::FTopKillerEntry, TInlineAllocator<ship_hud::top_killer_inline_capacity>>
        top_killers;
    ship_hud::FTeamKillMatrix team_kill_matrix;
};

struct FPlayerStatusDataCache {
    bool operator==(FPlayerStatusDataCache const& other) const noexcept = default;

    bool has_player_ship{false};
    FShipHealth health{};
    float speed{0.f};
    float target_speed{0.f};
    float energy{1.f};
    int32 points{0};
    sim::ShipFireRate fire_rate{sim::ShipFireRate::Single};
};

struct FPlayerFlightDataCache {
    bool operator==(FPlayerFlightDataCache const& other) const noexcept = default;

    bool has_player_ship{false};
    ship_hud::FFlightVectorDebugData flight_vector_debug{};
    FVector ship_velocity{};
    FVector target_velocity{};
    sim::player::FlightModelPreset flight_model_preset{};
    bool flight_model_customized{};
    float throttle{};
    sim::player::BoostBrakeState boost_brake_state{};
    FVector crosshair_origin{};
    FVector crosshair_direction{};
    FString selected_mapping_context;
};

#if WITH_EDITOR
struct FSampledSpeedDataCache {
    bool operator==(FSampledSpeedDataCache const& other) const noexcept = default;

    TArray<FVector2d> samples;
    int32 oldest_index{0};
};
#endif

struct FDataChanges {
    bool mission{false};
    bool entity_counts{false};
    bool player_status{false};
    bool player_flight{false};
#if WITH_EDITOR
    bool sampled_speed{false};
#endif
};
}

struct SPACEGAMEPRESENTATION_API FHUDManager {
    void configure(sim::HudSettings settings) { settings_ = settings; }
    auto settings() const -> sim::HudSettings const& { return settings_; }

    void initialise(sim::Mission const& new_mission,
                    sim::EntityLedger const& new_entity_ledger,
                    sim::capital_ships::Sim const& new_capitals,
                    sim::fighters::Sim const& new_fighters,
                    sim::turrets::Sim const& new_turrets,
                    sim::spinners::Sim const& new_spinners,
                    sim::SpatialQueryManager const& new_spatial_query_manager,
                    sim::player::Sim const* new_player_ship,
                    FLevelVisualConfig const& level_config,
                    UInstancedStaticMeshComponent* soft_target_instances = nullptr);
    void deactivate();
    void advance(double dt);
    void tick(PeriodicTickCountdown8::counter_type num_ticks);
    void force_sample();

    void register_hud(USimulationHudWidget& hud);
    void unregister_hud(USimulationHudWidget& hud);
    void set_selected_mapping_context(FString const& context_name);

    auto get_state() const noexcept -> EHUDManagerState { return state; }
    auto get_registered_hud_count() const noexcept -> int32 { return registered_huds.Num(); }
    auto has_mission_data_cache() const noexcept -> bool { return has_mission_data; }
    auto get_mission_data() const noexcept -> hud_manager::FMissionDataCache const& {
        return mission_data_buffers.current();
    }
    auto get_entity_count_data() const noexcept -> hud_manager::FEntityCountDataCache const& {
        return entity_count_data_buffers.current();
    }
    auto get_kill_data() const noexcept -> hud_manager::FKillDataCache const& { return kill_data_; }
    auto get_player_status_data() const noexcept -> hud_manager::FPlayerStatusDataCache const& {
        return player_status_data_buffers.current();
    }
    auto get_player_flight_data() const noexcept -> hud_manager::FPlayerFlightDataCache const& {
        return player_flight_data_buffers.current();
    }
#if WITH_EDITOR
    auto get_sampled_speed_data() const noexcept -> hud_manager::FSampledSpeedDataCache const& {
        return sampled_speed_data_buffers.current();
    }
#endif
  private:
    using FWorldSoftTargetCustomDataBuffer =
        TArray<float, TInlineAllocator<soft_target_world::custom_data_count * 2>>;

    struct FRegisteredHud {
        TWeakObjectPtr<USimulationHudWidget> hud;
        TWeakObjectPtr<UShipHudWidget> ship_hud;
        FEntityOverlayFrameStorePtr entity_overlay_frame_store;
        FEntityOverlayCollector entity_overlay_collector;
        FRadarFrameStorePtr radar_frame_store;
        presentation::SoftTargetState soft_targets{};
    };

    auto collect_data(PeriodicTickCountdown8::counter_type num_ticks) -> hud_manager::FDataChanges;
    auto display_batches() const -> std::array<presentation::AgentDisplayBatch, 4>;
    bool collect_mission_data();
    void read_mission_data(hud_manager::FMissionDataCache& out) const;
    bool collect_entity_count_data();
    void collect_kill_data();
    bool collect_player_status_data();
    bool collect_player_flight_data();
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
#if WITH_EDITOR
    bool collect_sampled_speed_data();
#endif

    void update_huds(hud_manager::FDataChanges const& changes);
    void synchronise_hud(USimulationHudWidget& hud) const;
    void update_mission_hud(USimulationHudWidget& hud) const;
    void update_entity_count_hud(USimulationHudWidget& hud) const;
    void update_player_status_hud(UShipHudWidget& hud) const;
    void update_player_flight_hud(UShipHudWidget& hud) const;
#if WITH_EDITOR
    void update_sampled_speed_hud(UShipHudWidget& hud) const;
#endif

    bool validate_player_ship_for_collection() const;

    sim::HudSettings settings_;

    EHUDManagerState state{EHUDManagerState::Disabled};
    TArray<FRegisteredHud> registered_huds;
    sim::player::Sim const* player_ship{nullptr};
    sim::Mission const* mission{nullptr};
    sim::EntityLedger const* entity_ledger{nullptr};
    sim::capital_ships::Sim const* capitals_{};
    sim::fighters::Sim const* fighters_{};
    sim::turrets::Sim const* turrets_{};
    sim::spinners::Sim const* spinners_{};
    sim::SpatialQueryManager const* spatial_query_manager{nullptr};
    sim::FixedTickLoop tick_loop_{};
    PeriodicTickCountdown8 update_timers;

    MultiBuffer<hud_manager::FMissionDataCache, 2> mission_data_buffers;
    MultiBuffer<hud_manager::FEntityCountDataCache, 2> entity_count_data_buffers;
    hud_manager::FKillDataCache kill_data_;
    MultiBuffer<hud_manager::FPlayerStatusDataCache, 2> player_status_data_buffers;
    MultiBuffer<hud_manager::FPlayerFlightDataCache, 2> player_flight_data_buffers;
    TArray<sim::EntityId, TInlineAllocator<ship_hud::top_killer_inline_capacity>>
        top_killer_ids_buffer;

    bool has_mission_data{false};
    FString selected_mapping_context;
    FEntityOverlaySettings entity_overlay_resources_;
    ioj::settings::EntityOverlay entity_overlay_settings_;
    FEntityOverlayStyle entity_overlay_style_;
    FEntityOverlayTeamColours entity_overlay_team_colours_;
    FEntityOverlayHealthMaximums entity_overlay_maximum_health_;
    TArray<EEntityOverlayObjectiveRole> entity_overlay_objective_roles_;
    ioj::settings::SoftTarget soft_target_selection_settings_;
    float soft_target_pulse_duration_{0.15f};
    float soft_target_fade_out_duration_{0.15f};
    float seconds_per_tick_{0.0f};
    TWeakObjectPtr<UInstancedStaticMeshComponent> soft_target_instances_;
    float soft_target_mesh_vertical_radius_{0.0f};
    bool warned_multiple_world_target_views_{false};
    ioj::settings::Radar radar_settings_;
    FRadarStyle radar_style_;
    FRadarContactColours radar_contact_colours_;

#if WITH_EDITOR
    MultiBuffer<hud_manager::FSampledSpeedDataCache, 2> sampled_speed_data_buffers;
    bool has_sampled_speed_data{false};
#endif
};

}
