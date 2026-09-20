#pragma once

#include <ioj/sim/player/flight_model_config.h>
#include <SpaceGame/ships/common/SpaceShipControllerInputs.h>
#include <SpaceGame/ships/player/ShipInputGestureRecognizer.h>

#include <CoreMinimal.h>

class ATestSpaceShip;
class ASpaceGamePlayerController;
class UEnhancedInputComponent;
class UInputAction;
class UInputMappingContext;
class IEnhancedInputSubsystemInterface;
struct FInputActionValue;
struct FShipControlContextTestAccess;

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
  private:
    void bind_actions();
    void remove_action_bindings();
    void neutralise_ship_input();
    void reset_flight_gesture_state();
    void select_flight_model_slot(::ioj::sim::player::FlightModelSlot slot);
    void publish_boost_intent();
    void add_mapping_context();
    void remove_mapping_context();

    void set_move_input(FInputActionValue const& value);
    void move_completed();
    void set_lateral_move_input(FInputActionValue const& value);
    void lateral_move_completed();
    void set_forward_move_input(FInputActionValue const& value);
    void forward_move_completed();
    void set_vertical_move_input(FInputActionValue const& value);
    void vertical_move_completed();
    void set_ship_2d_control_started();
    void set_ship_2d_control(FInputActionValue const& value);
    void ship_2d_control_completed();
    void set_ship_1d_control_x(FInputActionValue const& value);
    void set_ship_1d_control_y(FInputActionValue const& value);
    void select_flight_model_up();
    void select_flight_model_right();
    void select_flight_model_down();
    void select_flight_model_left();
    void start_sampling();
    void stop_sampling();
    void increase_desired_forward_velocity();
    void decrease_desired_forward_velocity();
    void turn(FInputActionValue const& value);
    void turn_completed();
    void engage_pointer_turn();
    void update_pointer_turn(FInputActionValue const& value);
    void disengage_pointer_turn();
    void publish_turn();
    void start_roll(FInputActionValue const& value);
    void roll(FInputActionValue const& value);
    void stop_roll(FInputActionValue const& value);
    void start_throttle(FInputActionValue const& value);
    void start_throttle_at(float input, double time_seconds);
    void set_throttle(FInputActionValue const& value);
    void set_throttle_value(float input);
    void stop_throttle();
    void stop_throttle_at(double time_seconds);
    void start_boost();
    void stop_boost();
    void start_brake();
    void start_brake_at(double time_seconds);
    void stop_brake();
    void stop_brake_at(double time_seconds);
    void cycle_input_mapping_context();
    void start_fire_laser();
    void stop_fire_laser();
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
    TWeakObjectPtr<UInputMappingContext> registered_mapping_;
    FVector2D turn_input_{FVector2D::ZeroVector};
    FVector2D pointer_turn_position_{FVector2D::ZeroVector};
    bool pointer_turn_engaged_{false};
    FShipInputGestureRecognizer throttle_gesture_{};
    FShipInputGestureRecognizer brake_gesture_{};
    bool throttle_press_active_{false};
    bool throttle_boost_active_{false};
    bool boost_press_active_{false};
    bool brake_press_active_{false};
    bool initialised_{false};
    bool bound_{false};

    friend struct FShipControlContextTestAccess;
};
