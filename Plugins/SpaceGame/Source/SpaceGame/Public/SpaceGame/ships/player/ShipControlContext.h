#pragma once

#include <ioj/sim/player/flight_model_config.h>
#include <SpaceGame/ships/common/SpaceShipControllerInputs.h>

#include <CoreMinimal.h>

class ATestSpaceShip;
class ASpaceGamePlayerController;
class UEnhancedInputComponent;
class UInputMappingContext;
class IEnhancedInputSubsystemInterface;
struct FInputActionValue;
struct FShipControlContextTestAccess;

struct SPACEGAME_API FShipControlContext {
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
    [[nodiscard]] auto active_mode_mapping() const -> UInputMappingContext* {
        return active_mode_mapping_.Get();
    }
  private:
    void bind_actions();
    void remove_action_bindings();
    void neutralise_ship_input();
    void select_flight_model_slot(::ioj::sim::player::FlightModelSlot slot);
    void select_starfox();
    void select_fighter();
    void select_skater();
    void select_gunship();
    void verify_mode_invariant() const;
    auto context_for_slot(::ioj::sim::player::FlightModelSlot slot) const -> UInputMappingContext*;
    auto get_ship() const -> ATestSpaceShip*;

    void set_forward_input(FInputActionValue const& value);
    void set_right_input(FInputActionValue const& value);
    void set_up_input(FInputActionValue const& value);
    void set_pitch_input(FInputActionValue const& value);
    void set_yaw_input(FInputActionValue const& value);
    void set_roll_input(FInputActionValue const& value);
    void set_accelerator(FInputActionValue const& value);
    void clear_forward_input();
    void clear_right_input();
    void clear_up_input();
    void clear_pitch_input();
    void clear_yaw_input();
    void clear_roll_input();
    void clear_accelerator();
    void start_boost();
    void stop_boost();
    void start_brake();
    void stop_brake();
    void start_emergency_brake();
    void stop_emergency_brake();
    void start_fire_primary();
    void stop_fire_primary();

    TWeakObjectPtr<ASpaceGamePlayerController> owner_;
    TWeakObjectPtr<UEnhancedInputComponent> input_component_;
    TWeakObjectPtr<UObject> input_subsystem_object_;
    TWeakObjectPtr<ATestSpaceShip> ship_;
    IEnhancedInputSubsystemInterface* input_subsystem_{nullptr};
    FSpaceShipControllerInputs const* input_{nullptr};
    TArray<uint32> binding_handles_;
    TWeakObjectPtr<UInputMappingContext> active_mode_mapping_;
    bool initialised_{false};
    bool bound_{false};

    friend struct FShipControlContextTestAccess;
};
