#pragma once

#include <SandboxCore/multi_buffer.h>
#include <SandboxCore/periodic_tick_countdown.h>
#include <SpaceGame/entities/TestEntityRegistry.h>
#include <SpaceGame/entities/TestEntityType.h>
#include <SpaceGame/missions/TestMissionMode.h>
#include <SpaceGame/missions/TestMissionState.h>
#include <SpaceGame/presentation/TestBatchGameUiData.h>
#include <SpaceGame/presentation/widgets/ShipHudKillData.h>
#include <SpaceGame/ships/common/ShipHealth.h>
#include <SpaceGame/ships/player/TestShipFireRate.h>
#include <SpaceGame/ships/player/TestSpaceShipControlMode.h>
#include <SpaceGame/ships/player/TestSpaceShipFlightMode.h>

#include <CoreMinimal.h>
#include <HAL/Platform.h>
#include <UObject/WeakObjectPtrTemplates.h>

struct FTestEntityRegistry;
struct FTestMissionManager;
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
    FLinearColor near_crosshair_colour{FLinearColor::Green};
    FLinearColor far_crosshair_colour{FLinearColor::Green};
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
    bool kill_data{false};
    bool player_status{false};
    bool player_flight{false};
#if WITH_EDITOR
    bool sampled_speed{false};
#endif
};
}

struct SPACEGAME_API FHUDManager {
    void initialise(FTestBatchGameUiUpdateFrequencies const& update_frequencies,
                    FTestMissionManager const& new_mission_manager,
                    FTestEntityRegistry const& new_entity_registry,
                    double update_tick_rate,
                    ml::test_space_ship::Simulation const* new_player_ship);
    void deactivate();
    void tick(FPeriodicTickCountdown8::counter_type num_ticks);
    void force_sample();

    void register_hud(UShipHudWidget& hud);
    void unregister_hud(UShipHudWidget& hud);
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
    auto collect_data(FPeriodicTickCountdown8::counter_type num_ticks)
        -> ml::hud_manager::FDataChanges;
    bool collect_mission_data();
    void read_mission_data(ml::hud_manager::FMissionDataCache& out) const;
    bool collect_entity_count_data();
    bool collect_kill_data();
    bool collect_player_status_data();
    bool collect_player_flight_data();
#if WITH_EDITOR
    bool collect_sampled_speed_data();
#endif

    void update_huds(ml::hud_manager::FDataChanges const& changes);
    void synchronise_hud(UShipHudWidget& hud) const;
    void update_mission_hud(UShipHudWidget& hud) const;
    void update_entity_count_hud(UShipHudWidget& hud) const;
    void update_kill_data_hud(UShipHudWidget& hud) const;
    void update_player_status_hud(UShipHudWidget& hud) const;
    void update_player_flight_hud(UShipHudWidget& hud) const;
#if WITH_EDITOR
    void update_sampled_speed_hud(UShipHudWidget& hud) const;
#endif

    bool validate_player_ship_for_collection() const;

    EHUDManagerState state{EHUDManagerState::Disabled};
    TArray<TWeakObjectPtr<UShipHudWidget>> registered_huds;
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

#if WITH_EDITOR
    ml::MultiBuffer<ml::hud_manager::FSampledSpeedDataCache, 2> sampled_speed_data_buffers;
    bool has_sampled_speed_data{false};
#endif
};
