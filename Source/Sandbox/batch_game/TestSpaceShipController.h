#pragma once

#include <Sandbox/batch_game/TestShipFireRate.h>
#include <Sandbox/input/EnhancedInputMixin.hpp>
#include <Sandbox/logging/ActorLoggingConfig.h>
#include <Sandbox/players/LaserFiringState.h>
#include <Sandbox/players/SpaceShipControllerInputs.h>
#include <SandboxCore/error_msg.h>
#include <SandboxCore/periodic_countdown_timers.h>

#include <CoreMinimal.h>
#include <GameFramework/PlayerController.h>

#include <span>

#include "TestSpaceShipController.generated.h"

class UShipHudWidget;
class UInputAction;
class ATestSpaceShip;
class ATestMissionManager;
class ATestEntityRegistry;
class UTestBatchGameUiData;
struct FShipHealth;

struct FTestSpaceShipControllerUiTimerIndices {
    static constexpr int32 entity_count{0};
    static constexpr int32 count{1};
};

UCLASS()
class ATestSpaceShipController
    : public APlayerController
    , public ml::EnhancedInputMixin {
    GENERATED_BODY()
  public:
    struct UICache {
        int32 player_kills{0};
    };

    using Pawn = ATestSpaceShip;

    ATestSpaceShipController();

    void SetupInputComponent() override;
    void Tick(float dt) override;
  protected:
    void BeginPlay() override;
    void OnPossess(APawn* in_pawn) override;
    void OnUnPossess() override;
    void EndPlay(EEndPlayReason::Type const reason);

    void initialise_hud();
    void update_crosshair_positions(ATestSpaceShip const& ship);
    void update_lock_on_widget(ATestSpaceShip const& ship);
    void update_input_widgets(ATestSpaceShip const& ship);
    void update_entity_count_table();

    auto get_pawn() -> Pawn&;

    // Input
    void set_mapping_context(UInputMappingContext const* context);

    // Movement
    UFUNCTION()
    void set_move_input(FInputActionValue const& value);
    UFUNCTION()
    void move_completed();
    UFUNCTION()
    void set_lateral_move_input(FInputActionValue const& value);
    UFUNCTION()
    void lateral_move_completed();
    UFUNCTION()
    void set_vertical_move_input(FInputActionValue const& value);
    UFUNCTION()
    void vertical_move_completed();
    UFUNCTION()
    void set_ship_2d_control_started();
    UFUNCTION()
    void set_ship_2d_control(FInputActionValue const& value);
    UFUNCTION()
    void ship_2d_control_completed();
    UFUNCTION()
    void set_ship_1d_control_x(FInputActionValue const& value);
    UFUNCTION()
    void set_ship_1d_control_y(FInputActionValue const& value);
    UFUNCTION()
    void cycle_next_control_mode();
    UFUNCTION()
    void cycle_previous_control_mode();
    UFUNCTION()
    void start_sampling();
    UFUNCTION()
    void stop_sampling();
    UFUNCTION()
    void turn(FInputActionValue const& value);
    UFUNCTION()
    void turn_completed(FInputActionValue const& value);
    UFUNCTION()
    void start_roll(FInputActionValue const& value);
    UFUNCTION()
    void roll(FInputActionValue const& value);
    UFUNCTION()
    void stop_roll(FInputActionValue const& value);
    UFUNCTION()
    void start_boost(FInputActionValue const& value);
    UFUNCTION()
    void stop_boost(FInputActionValue const& value);
    UFUNCTION()
    void start_brake(FInputActionValue const& value);
    UFUNCTION()
    void stop_brake(FInputActionValue const& value);
    UFUNCTION()
    void cycle_input_mapping_context();

    // Combat
    UFUNCTION()
    void start_fire_laser();
    UFUNCTION()
    void stop_fire_laser();
    UFUNCTION()
    void fire_bomb(FInputActionValue const& value);

    UFUNCTION()
    void cycle_prev_fire_rate();
    UFUNCTION()
    void cycle_next_fire_rate();

    // UI
    void on_health_changed(FShipHealth value);
    void on_speed_changed(float value);
    void on_target_speed_changed(float value);
    void on_energy_changed(float value);
    void on_bombs_changed(int32 value);
    UFUNCTION()
    void on_laser_firing_mode_changed(ELaserFiringState mode);
    UFUNCTION()
    void on_lock_on_acquired(AActor* target);
    UFUNCTION()
    void on_ship_fire_rate_changed(ETestShipFireRate const value);
    void on_player_ship_died();

#if WITH_EDITOR
    void on_speed_sampled(std::span<FVector2d> samples, int32 oldest_index);
#endif

    // Mission
    void on_mission_manager_ready(ATestMissionManager const& manager);
    void initialise_from_mission_manager(ATestMissionManager const& manager);
    void on_mission_update(ATestMissionManager const& manager);
    void on_mission_ended(ATestMissionManager const& manager);
    auto make_mission_status_message(ATestMissionManager const& manager) const -> FString;

    // Misc
    void screenshot_tick(float dt);

    // UI
    UPROPERTY(EditAnywhere, Category = "Sandbox|UI")
    TSubclassOf<UShipHudWidget> hud_widget_class;
    UPROPERTY(EditAnywhere, Category = "Sandbox|UI")
    TObjectPtr<UTestBatchGameUiData> ui_data{nullptr};
    UPROPERTY(VisibleAnywhere, Category = "Sandbox|UI")
    UShipHudWidget* hud_widget{nullptr};
    UPROPERTY(EditAnywhere, Category = "Sandbox|UI")
    float near_cursor_distance{3000.f};
    UPROPERTY(EditAnywhere, Category = "Sandbox|UI")
    float far_cursor_distance{6000.f};

    UPROPERTY(EditAnywhere, Category = "Sandbox|Input")
    FSpaceShipControllerInputs input;

    UPROPERTY(EditAnywhere, Category = "Sandbox|Input")
    UInputAction* lateral_move_input{nullptr};
    UPROPERTY(EditAnywhere, Category = "Sandbox|Input")
    UInputAction* vertical_move_input{nullptr};
    UPROPERTY(EditAnywhere, Category = "Sandbox|Input")
    UInputAction* sample_and_hold_input{nullptr};
    UPROPERTY(EditAnywhere, Category = "Sandbox|Input")
    UInputAction* ship_2d_control{nullptr};
    UPROPERTY(EditAnywhere, Category = "Sandbox|Input")
    UInputAction* ship_1d_control_x{nullptr};
    UPROPERTY(EditAnywhere, Category = "Sandbox|Input")
    UInputAction* ship_1d_control_y{nullptr};
    UPROPERTY(EditAnywhere, Category = "Sandbox|Input")
    UInputAction* cycle_next_control_mode_input{nullptr};
    UPROPERTY(EditAnywhere, Category = "Sandbox|Input")
    UInputAction* cycle_previous_control_mode_input{nullptr};

    UPROPERTY(EditAnywhere, Category = "Sandbox|Input")
    int32 input_mapping_context_index{0};

    // Mission state
    UPROPERTY(EditAnywhere, Category = "Sandbox|Mission")
    TObjectPtr<ATestMissionManager> mission_manager{nullptr};
    UPROPERTY(VisibleAnywhere, Category = "Sandbox|Mission")
    TObjectPtr<ATestEntityRegistry> entity_registry{nullptr};
    FDelegateHandle on_mission_ended_handle;
    FDelegateHandle on_mission_update_handle;
    FDelegateHandle on_mission_manager_ready_handle;

    FPeriodicCountdownTimers ui_timers;
    ml::FErrorMsg error_msg;

    // Player state
    UICache ui_cache;

    UPROPERTY(EditAnywhere, Category = "SpaceShip|Logging")
    FActorLoggingConfig log_config{1.f};

    UPROPERTY(EditAnywhere, Category = "Sandbox|Screenshot")
    float screenshot_period{-1.f};
    float screenshot_accumulator{0.f};

#if WITH_EDITORONLY_DATA
    UPROPERTY(EditAnywhere, Category = "Sandbox|Debug")
    bool debug_crosshair{false};
#endif
};
