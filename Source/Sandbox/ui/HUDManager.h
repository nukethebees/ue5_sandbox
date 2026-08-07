#pragma once

#include <Sandbox/batch_game/SimulationClockInterface.h>
#include <Sandbox/batch_game/TestShipFireRate.h>
#include <Sandbox/health/ShipHealth.h>
#include <Sandbox/players/LaserFiringState.h>
#include <SandboxCore/periodic_tick_countdown.h>

#include <CoreMinimal.h>
#include <Delegates/IDelegateInstance.h>
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
    static constexpr int32 mission_status{1};
    static constexpr int32 entity_counts{2};
    static constexpr int32 count{3};
};

struct SANDBOX_API FHUDManager {
    using SimulationClockInterface = ml::test_batch_orchestrator::SimulationClockInterface;

    void initialise(UShipHudWidget& new_hud_widget,
                    UTestBatchGameUiData const& ui_data,
                    ATestMissionManager const& new_mission_manager,
                    ATestEntityRegistry const& new_entity_registry,
                    SimulationClockInterface const simulation_clock,
                    ATestSpaceShipController& new_player_controller,
                    ATestSpaceShip* new_player_ship,
                    float new_near_cursor_distance,
                    float new_far_cursor_distance,
                    bool new_debug_crosshair);
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
    void on_mission_started(ATestMissionManager const& manager);
    void on_enemies_killed(ATestMissionManager const& manager);
    void on_surviving_entity_health_updated(ATestMissionManager const& manager);
    void on_mission_ended(ATestMissionManager const& manager);
    void remove_mission_delegates();

    EHUDManagerState state{EHUDManagerState::Disabled};
    TWeakObjectPtr<UShipHudWidget> hud_widget;
    TWeakObjectPtr<ATestSpaceShipController> player_controller;
    TWeakObjectPtr<ATestSpaceShip> player_ship;
    ATestMissionManager const* mission_manager{nullptr};
    ATestEntityRegistry const* entity_registry{nullptr};
    SimulationClockInterface simulation_clock{};
    float near_cursor_distance{0.f};
    float far_cursor_distance{0.f};
    bool debug_crosshair{false};
    FPeriodicTickCountdown8 update_timers;
    FDelegateHandle on_mission_started_handle;
    FDelegateHandle on_enemies_killed_handle;
    FDelegateHandle on_surviving_entity_health_updated_handle;
    FDelegateHandle on_mission_ended_handle;
    bool mission_started_pending{false};
    bool enemies_killed_pending{false};
    bool surviving_entity_health_pending{false};
    bool mission_ended_pending{false};
    ml::hud_manager::FLogOnceFlags has_logged;
};
