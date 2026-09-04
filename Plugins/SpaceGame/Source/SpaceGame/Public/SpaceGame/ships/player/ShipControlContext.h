#pragma once

#include <SpaceGame/ships/common/SpaceShipControllerInputs.h>

#include <CoreMinimal.h>

class ATestSpaceShip;
class ASpaceGamePlayerController;
class UEnhancedInputComponent;
class UInputAction;
class IEnhancedInputSubsystemInterface;
struct FInputActionValue;

struct SPACEGAME_API FShipControlContext {
  public:
    FShipControlContext() = default;
    FShipControlContext(FShipControlContext const&) = delete;
    FShipControlContext(FShipControlContext&&) = delete;
    auto operator=(FShipControlContext const&) -> FShipControlContext& = delete;
    auto operator=(FShipControlContext&&) -> FShipControlContext& = delete;

    auto initialise(ASpaceGamePlayerController& owner,
                    UEnhancedInputComponent& input_component,
                    IEnhancedInputSubsystemInterface& input_subsystem,
                    FSpaceShipControllerInputs const& input) -> bool;
    auto bind() -> bool;
    void unbind();
    void shutdown();

    void set_ship(ATestSpaceShip* ship);

    [[nodiscard]] auto can_bind() const -> bool;
    [[nodiscard]] auto is_initialised() const noexcept -> bool { return initialised_; }
    [[nodiscard]] auto is_bound() const noexcept -> bool { return bound_; }
    [[nodiscard]] auto get_mapping_context_index() const noexcept -> int32 {
        return mapping_context_index_;
    }
  private:
    void bind_actions();
    void remove_action_bindings();
    void neutralise_ship_input();
    void add_selected_mapping_context();
    void remove_selected_mapping_context();

    void set_move_input(FInputActionValue const& value);
    void move_completed();
    void set_lateral_move_input(FInputActionValue const& value);
    void lateral_move_completed();
    void set_vertical_move_input(FInputActionValue const& value);
    void vertical_move_completed();
    void set_ship_2d_control_started();
    void set_ship_2d_control(FInputActionValue const& value);
    void ship_2d_control_completed();
    void set_ship_1d_control_x(FInputActionValue const& value);
    void set_ship_1d_control_y(FInputActionValue const& value);
    void cycle_next_control_mode();
    void cycle_previous_control_mode();
    void start_sampling();
    void stop_sampling();
    void turn(FInputActionValue const& value);
    void turn_completed();
    void start_roll(FInputActionValue const& value);
    void roll(FInputActionValue const& value);
    void stop_roll(FInputActionValue const& value);
    void start_boost();
    void stop_boost();
    void start_brake();
    void stop_brake();
    void cycle_input_mapping_context();
    void start_fire_laser();
    void stop_fire_laser();
    void fire_bomb();
    void cycle_prev_fire_rate();
    void cycle_next_fire_rate();

    auto get_ship() const -> ATestSpaceShip*;

    TWeakObjectPtr<ASpaceGamePlayerController> owner_;
    TWeakObjectPtr<UEnhancedInputComponent> input_component_;
    TWeakObjectPtr<UObject> input_subsystem_object_;
    TWeakObjectPtr<ATestSpaceShip> ship_;
    IEnhancedInputSubsystemInterface* input_subsystem_{nullptr};
    FSpaceShipControllerInputs const* input_{nullptr};
    TArray<uint32> binding_handles_;
    int32 mapping_context_index_{0};
    bool initialised_{false};
    bool bound_{false};
};
