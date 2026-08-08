#pragma once

#include <Sandbox/batch_game/SimulationClockInterface.h>
#include <Sandbox/batch_game/test_entity_registry/TestEntityUniqueId.h>
#include <Sandbox/batch_game/TestEntityType.h>
#include <Sandbox/batch_game/TestMissionMode.h>
#include <Sandbox/batch_game/TestMissionState.h>
#include <Sandbox/batch_game/TestShipFireRate.h>
#include <Sandbox/health/ShipHealth.h>
#include <Sandbox/players/LaserFiringState.h>
#include <Sandbox/ui/HudCrosshairDistances.h>
#include <SandboxCore/multi_buffer.h>
#include <SandboxCore/periodic_tick_countdown.h>

#include <CoreMinimal.h>
#include <HAL/Platform.h>
#include <UObject/WeakObjectPtrTemplates.h>

#include <span>

class ATestSpaceShipController;
class ATestEntityRegistry;
class ATestMissionManager;
class ATestSpaceShip;
class AActor;
class UTestBatchGameUiData;
class UShipHudWidget;

namespace ml::hud_manager {
struct FLogOnceFlags {
    void reset() noexcept {
        tick_dependencies_error_logged = false;
        player_ship_error_logged = false;
        player_ship_unique_id_error_logged = false;
        player_hud_dependencies_error_logged = false;
        mission_status_dependencies_error_logged = false;
        player_status_dependencies_error_logged = false;
        entity_counts_dependencies_error_logged = false;
        crosshair_dependencies_error_logged = false;
        lock_on_dependencies_error_logged = false;
        input_widgets_dependencies_error_logged = false;
    }

    bool tick_dependencies_error_logged{false};
    bool player_ship_error_logged{false};
    bool player_ship_unique_id_error_logged{false};
    bool player_hud_dependencies_error_logged{false};
    bool mission_status_dependencies_error_logged{false};
    bool player_status_dependencies_error_logged{false};
    bool entity_counts_dependencies_error_logged{false};
    bool crosshair_dependencies_error_logged{false};
    bool lock_on_dependencies_error_logged{false};
    bool input_widgets_dependencies_error_logged{false};
};
}

enum class EHUDManagerState : uint8 {
    Disabled,
    Active,
};

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
};

struct FMissionStatusDataCache {
    bool operator==(FMissionStatusDataCache const& other) const noexcept = default;

    ETestMissionState mission_state{ETestMissionState::NotStarted};
    float mission_stopwatch{0.f};
    float time_remaining{0.f};
    int32 enemies_remaining{0};
    TArray<FShipHealth> surviving_entity_health;
};

struct FMissionDataCache {
    bool operator==(FMissionDataCache const& other) const noexcept = default;

    FMissionStaticDataCache static_data;
    FMissionStatusDataCache status_data;
};
}

struct SANDBOX_API FHUDManager {
    using SimulationClockInterface = ml::test_batch_orchestrator::SimulationClockInterface;

    void initialise(UShipHudWidget& new_hud_widget,
                    UTestBatchGameUiData const& new_ui_data,
                    ATestMissionManager const& new_mission_manager,
                    ATestEntityRegistry const& new_entity_registry,
                    SimulationClockInterface const simulation_clock,
                    ATestSpaceShipController& new_player_controller,
                    ATestSpaceShip* new_player_ship);
    void deactivate();
    void tick();

    void set_player_ship(ATestSpaceShip* new_player_ship) noexcept;
    void clear_player_ship() noexcept;
    void set_selected_mapping_context(FString const& context_name);
    auto get_state() const noexcept -> EHUDManagerState { return state; }
  private:
    bool validate_player_ship_for_update();
    void update_player_hud();
    void update_mission_status();
    void read_mission_data(ml::hud_manager::FMissionDataCache& out) const;
    void update_entity_counts();
    void update_player_status();
    void update_crosshair_positions(ATestSpaceShip const& ship);
    void update_lock_on_widget(ATestSpaceShip const& ship);
    void update_input_widgets(ATestSpaceShip const& ship);
    void on_health_changed(FShipHealth value);
    void on_speed_changed(float value);
    void on_target_speed_changed(float value);
    void on_energy_changed(float value);
    void on_bombs_changed(int32 value);
    void on_ship_fire_rate_changed(ETestShipFireRate value);
    void on_laser_firing_mode_changed(ELaserFiringState mode);
    void on_lock_on_acquired(AActor* target);
#if WITH_EDITOR
    void on_speed_sampled(std::span<FVector2d> samples, int32 oldest_index);
#endif
    void bind_player_ship_delegates();
    void remove_player_ship_delegates();

    EHUDManagerState state{EHUDManagerState::Disabled};
    TWeakObjectPtr<UShipHudWidget> hud_widget;
    TWeakObjectPtr<ATestSpaceShipController> player_controller;
    TWeakObjectPtr<ATestSpaceShip> player_ship;
    UTestBatchGameUiData const* ui_data{nullptr};
    ATestMissionManager const* mission_manager{nullptr};
    ATestEntityRegistry const* entity_registry{nullptr};
    FPeriodicTickCountdown8 update_timers;
    ml::MultiBuffer<ml::hud_manager::FMissionDataCache, 2> mission_data_buffers;
    bool has_mission_data{false};
    ml::hud_manager::FLogOnceFlags has_logged;
};
