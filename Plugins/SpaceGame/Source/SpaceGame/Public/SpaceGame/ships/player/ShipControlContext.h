#pragma once

#include <SpaceGame/ships/common/SpaceShipControllerInputs.h>

#include <CoreMinimal.h>

class UEnhancedInputComponent;
class UInputMappingContext;
class IEnhancedInputSubsystemInterface;
struct FInputActionValue;

namespace ioj {
class ATestSpaceShip;
class ASpaceGamePlayerController;

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
    // Require successful can_bind() preflight on the game thread.
    void bind();
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
    auto get_ship() const -> ATestSpaceShip*;

    void set_pitch_input(FInputActionValue const& value);
    void start_fire_primary();

    TWeakObjectPtr<ASpaceGamePlayerController> owner_;
    TWeakObjectPtr<UEnhancedInputComponent> input_component_;
    TWeakObjectPtr<UObject> input_subsystem_object_;
    TWeakObjectPtr<ATestSpaceShip> ship_;
    IEnhancedInputSubsystemInterface* input_subsystem_{nullptr};
    FSpaceShipControllerInputs const* input_{nullptr};
    TArray<uint32> binding_handles_;
    bool initialised_{false};
    bool bound_{false};
};

} // namespace ioj
