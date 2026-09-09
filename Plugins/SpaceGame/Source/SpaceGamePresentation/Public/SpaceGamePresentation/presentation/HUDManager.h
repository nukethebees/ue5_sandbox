#pragma once

#include <SandboxCore/multi_buffer.h>
#include <SandboxCore/periodic_tick_countdown.h>
#include <SandboxUI/EntityOverlay/EntityOverlayFrameStore.h>
#include <SandboxUI/EntityOverlay/EntityOverlayTypes.h>
#include <SandboxUI/Radar/RadarFrameStore.h>
#include <SandboxUI/Radar/RadarTypes.h>
#include <SpaceGamePresentation/presentation/EntityOverlaySource.h>
#include <SpaceGamePresentation/presentation/HudUpdateSettings.h>
#include <SpaceGamePresentation/presentation/LevelPresentationSettings.h>
#include <SpaceGamePresentation/presentation/RadarSource.h>
#include <SpaceGamePresentation/presentation/widgets/ShipHudKillData.h>
#include <SpaceGameSimulation/entities/TestEntityRegistry.h>
#include <SpaceGameSimulation/entities/TestEntityType.h>
#include <SpaceGameSimulation/missions/TestMissionMode.h>
#include <SpaceGameSimulation/missions/TestMissionState.h>
#include <SpaceGameSimulation/ships/common/ShipHealth.h>
#include <SpaceGameSimulation/ships/player/TestShipFireRate.h>
#include <SpaceGameSimulation/ships/player/TestSpaceShipControlMode.h>
#include <SpaceGameSimulation/ships/player/TestSpaceShipFlightMode.h>

#include <CoreMinimal.h>
#include <HAL/Platform.h>
#include <UObject/WeakObjectPtrTemplates.h>

struct FTestEntityRegistry;
struct FTestMissionManager;
struct FLevelVisualConfig;
class USimulationHudWidget;
class UShipHudWidget;

namespace ml::test_space_ship {
struct Simulation;
}

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
    static constexpr int32 count{3};
};

namespace ml::hud_manager {
struct FMissionStaticDataCache {
    bool operator==(FMissionStaticDataCache const& other) const noexcept = default;

    ETestMissionMode mission_mode{ETestMissionMode::None};
    TArray<TestEntityUniqueId> surviving_entity_ids;
    TArray<ETestEntityType> surviving_entity_types;
    TArray<TestEntityUniqueId> required_kill_entity_ids;
    TArray<ETestEntityType> required_kill_entity_types;
};

struct FMissionStatusDataCache {
    bool operator==(FMissionStatusDataCache const& other) const noexcept = default;

    ETestMissionState mission_state{ETestMissionState::NotStarted};
    float mission_stopwatch{0.f};
    float time_remaining{0.f};
    int32 enemies_remaining{0};
    TArray<FShipHealth> surviving_entity_health;
    TArray<FShipHealth> required_kill_entity_health;
};

struct FMissionDataCache {
    bool operator==(FMissionDataCache const& other) const noexcept = default;

    FMissionStaticDataCache static_data;
    FMissionStatusDataCache status_data;
};

struct FEntityCountDataCache {
    bool operator==(FEntityCountDataCache const& other) const noexcept = default;

    FTestEntityRegistry::EntityCounts alive_per_team_and_type{};
};

struct FKillDataCache {
    bool operator==(FKillDataCache const& other) const noexcept = default;

    ml::ship_hud::FTopKillerEntries top_killers;
    ml::ship_hud::FTeamKillMatrix team_kill_matrix;
};

struct FPlayerStatusDataCache {
    bool operator==(FPlayerStatusDataCache const& other) const noexcept = default;

    bool has_player_ship{false};
    FShipHealth health{};
    float speed{0.f};
    float target_speed{0.f};
    float energy{1.f};
    int32 points{0};
    ETestShipFireRate fire_rate{ETestShipFireRate::Single};
    bool crosshair_targeting{false};
};

struct FPlayerFlightDataCache {
    bool operator==(FPlayerFlightDataCache const& other) const noexcept = default;

    bool has_player_ship{false};
    FVector2D turning{};
    FVector2D moving{};
    FVector2D desired_velocity_scale{};
    FVector ship_velocity{};
    FVector target_velocity{};
    ETestSpaceShipControlMode control_mode{};
    ETestSpaceShipFlightMode flight_mode{};
    FVector crosshair_origin{};
    FVector crosshair_direction{};
    bool has_lock_on_target{false};
    FVector lock_on_target_position{};
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
    void initialise(FTestBatchGameUiUpdateFrequencies const& update_frequencies,
                    FTestMissionManager const& new_mission_manager,
                    FTestEntityRegistry const& new_entity_registry,
                    double update_tick_rate,
                    ml::test_space_ship::Simulation const* new_player_ship,
                    FLevelVisualConfig const& level_config,
                    FEntityOverlaySettings const& entity_overlay_settings,
                    FRadarSettings const& radar_settings);
    void deactivate();
    void tick(FPeriodicTickCountdown8::counter_type num_ticks);
    void force_sample();

    void register_hud(USimulationHudWidget& hud);
    void unregister_hud(USimulationHudWidget& hud);
    void set_selected_mapping_context(FString const& context_name);

    auto get_state() const noexcept -> EHUDManagerState { return state; }
    auto get_registered_hud_count() const noexcept -> int32 { return registered_huds.Num(); }
    auto has_mission_data_cache() const noexcept -> bool { return has_mission_data; }
    auto get_mission_data() const noexcept -> ml::hud_manager::FMissionDataCache const& {
        return mission_data_buffers.current();
    }
    auto get_entity_count_data() const noexcept -> ml::hud_manager::FEntityCountDataCache const& {
        return entity_count_data_buffers.current();
    }
    auto get_kill_data() const noexcept -> ml::hud_manager::FKillDataCache const& {
        return kill_data_buffers.current();
    }
    auto get_player_status_data() const noexcept -> ml::hud_manager::FPlayerStatusDataCache const& {
        return player_status_data_buffers.current();
    }
    auto get_player_flight_data() const noexcept -> ml::hud_manager::FPlayerFlightDataCache const& {
        return player_flight_data_buffers.current();
    }
#if WITH_EDITOR
    auto get_sampled_speed_data() const noexcept -> ml::hud_manager::FSampledSpeedDataCache const& {
        return sampled_speed_data_buffers.current();
    }
#endif
  private:
    struct FRegisteredHud {
        TWeakObjectPtr<USimulationHudWidget> hud;
        TWeakObjectPtr<UShipHudWidget> ship_hud;
        FEntityOverlayFrameStorePtr entity_overlay_frame_store;
        FEntityOverlayCollector entity_overlay_collector;
        FRadarFrameStorePtr radar_frame_store;
        FRegistryEntityHandle soft_target{};
        float soft_target_range_progress{0.0f};
        float soft_target_radius_pixels{0.0f};
        float soft_target_pulse_remaining{0.0f};
        bool soft_target_in_range{false};
        FRegistryEntityHandle fading_soft_target{};
        float fading_soft_target_range_progress{0.0f};
        float fading_soft_target_radius_pixels{0.0f};
        float fading_soft_target_visibility_remaining{0.0f};
        bool fading_soft_target_in_range{false};
    };

    auto collect_data(FPeriodicTickCountdown8::counter_type num_ticks)
        -> ml::hud_manager::FDataChanges;
    bool collect_mission_data();
    void read_mission_data(ml::hud_manager::FMissionDataCache& out) const;
    bool collect_entity_count_data();
    void collect_kill_data();
    bool collect_player_status_data();
    bool collect_player_flight_data();
    void update_entity_overlays(float delta_seconds);
    void update_entity_overlay_objective_roles();
    void update_entity_overlay(FRegisteredHud& registration, float delta_seconds);
    void update_radars();
    void update_radar(FRegisteredHud& registration);
#if WITH_EDITOR
    bool collect_sampled_speed_data();
#endif

    void update_huds(ml::hud_manager::FDataChanges const& changes);
    void synchronise_hud(USimulationHudWidget& hud) const;
    void update_mission_hud(USimulationHudWidget& hud) const;
    void update_entity_count_hud(USimulationHudWidget& hud) const;
    void update_player_status_hud(UShipHudWidget& hud) const;
    void update_player_flight_hud(UShipHudWidget& hud) const;
#if WITH_EDITOR
    void update_sampled_speed_hud(UShipHudWidget& hud) const;
#endif

    bool validate_player_ship_for_collection() const;

    EHUDManagerState state{EHUDManagerState::Disabled};
    TArray<FRegisteredHud> registered_huds;
    ml::test_space_ship::Simulation const* player_ship{nullptr};
    FTestMissionManager const* mission_manager{nullptr};
    FTestEntityRegistry const* entity_registry{nullptr};
    FPeriodicTickCountdown8 update_timers;

    ml::MultiBuffer<ml::hud_manager::FMissionDataCache, 2> mission_data_buffers;
    ml::MultiBuffer<ml::hud_manager::FEntityCountDataCache, 2> entity_count_data_buffers;
    ml::MultiBuffer<ml::hud_manager::FKillDataCache, 2> kill_data_buffers;
    ml::MultiBuffer<ml::hud_manager::FPlayerStatusDataCache, 2> player_status_data_buffers;
    ml::MultiBuffer<ml::hud_manager::FPlayerFlightDataCache, 2> player_flight_data_buffers;
    TArray<TestEntityUniqueId, TInlineAllocator<ml::ship_hud::FTopKillerEntries::minimum_size>>
        top_killer_ids_buffer;

    bool has_mission_data{false};
    FString selected_mapping_context;
    FEntityOverlaySettings entity_overlay_settings_;
    FEntityOverlayStyle entity_overlay_style_;
    FEntityOverlayTeamColours entity_overlay_team_colours_;
    FEntityOverlayHealthMaximums entity_overlay_maximum_health_;
    TArray<EEntityOverlayObjectiveRole> entity_overlay_objective_roles_;
    FSoftTargetSelectionSettings soft_target_selection_settings_;
    float soft_target_pulse_duration_{0.15f};
    float soft_target_fade_out_duration_{0.15f};
    float seconds_per_tick_{0.0f};
    FRadarSettings radar_settings_;
    FRadarStyle radar_style_;
    FRadarContactColours radar_contact_colours_;

#if WITH_EDITOR
    ml::MultiBuffer<ml::hud_manager::FSampledSpeedDataCache, 2> sampled_speed_data_buffers;
    bool has_sampled_speed_data{false};
#endif
};
