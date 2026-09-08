#pragma once

#include <SpaceGame/ships/player/BenchmarkControlContext.h>
#include <SpaceGame/ships/player/GlobalPlayerInput.h>
#include <SpaceGame/ships/player/ObserverControlContext.h>
#include <SpaceGame/ships/player/PlayerControlContext.h>
#include <SpaceGame/ships/player/ShipControlContext.h>

struct SPACEGAME_API FPlayerControlContexts {
    auto initialise(ASpaceGamePlayerController& owner,
                    UEnhancedInputComponent& component,
                    IEnhancedInputSubsystemInterface& subsystem,
                    FSpaceShipControllerInputs const& ship_input,
                    FObserverControlInputs const& observer_input,
                    FBenchmarkControlInputs const& benchmark_input,
                    FGlobalControlInputs const& global_input) -> bool;
    void shutdown();
    auto set_control_context(EPlayerControlContext context) -> bool;
    auto can_bind_context(EPlayerControlContext context) const -> bool;
    void set_ship(ATestSpaceShip* ship);
    void set_camera(ACameraActor* camera);
    auto get_active_context() const -> EPlayerControlContext { return active_context_; }
    auto is_observer_bound() const -> bool { return observer_.is_bound(); }
    auto get_observer_speed() const -> float { return observer_.get_movement_speed(); }
  private:
    friend struct FPlayerControlContextsTestAccess;
    auto bind_context(EPlayerControlContext context) -> bool;
    void unbind_context(EPlayerControlContext context);

    FShipControlContext ship_;
    FObserverControlContext observer_;
    FBenchmarkControlContext benchmark_;
    FGlobalPlayerInput global_;
    EPlayerControlContext active_context_{EPlayerControlContext::None};
    TWeakObjectPtr<UEnhancedInputComponent> component_;
    TWeakObjectPtr<UObject> subsystem_object_;
    TWeakObjectPtr<ATestSpaceShip> player_ship_;
    TWeakObjectPtr<ACameraActor> camera_;
    bool initialised_{false};
#if WITH_DEV_AUTOMATION_TESTS
    uint8 fail_bind_mask_{0};
#endif
};
