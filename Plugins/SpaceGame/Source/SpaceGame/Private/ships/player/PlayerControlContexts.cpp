#include <SpaceGame/ships/player/PlayerControlContexts.h>

#include <SpaceGame/ships/player/SpaceGamePlayerController.h>
#include <SpaceGame/ships/player/TestSpaceShip.h>
#include <SpaceGame/support/logging/SandboxLogCategories.h>

#include <Camera/CameraActor.h>
#include <EnhancedInputComponent.h>
#include <EnhancedInputSubsystemInterface.h>

auto FPlayerControlContexts::initialise(ASpaceGamePlayerController& owner,
                                        UEnhancedInputComponent& component,
                                        IEnhancedInputSubsystemInterface& subsystem,
                                        FSpaceShipControllerInputs const& ship_input,
                                        FObserverControlInputs const& observer_input,
                                        FBenchmarkControlInputs const& benchmark_input,
                                        FGlobalControlInputs const& global_input) -> bool {
    auto* const subsystem_object{Cast<UObject>(&subsystem)};
    if (initialised_ && component_.Get() == &component &&
        subsystem_object_.Get() == subsystem_object) {
        return true;
    }
    auto const previous_context{active_context_};
    auto const global_enabled{initialised_ ? global_.is_mapping_enabled() : true};
    auto const ship{player_ship_};
    auto const camera{camera_};
    shutdown();
    set_ship(ship.Get());
    set_camera(camera.Get());

    auto const global_ready{global_.initialise(owner, component, subsystem, global_input)};
    auto const ship_ready{ship_.initialise(owner, component, subsystem, ship_input)};
    auto const observer_ready{observer_.initialise(owner, component, subsystem, observer_input)};
    auto const benchmark_ready{benchmark_.initialise(owner, component, subsystem, benchmark_input)};
    if (!global_ready || !ship_ready || !observer_ready || !benchmark_ready) {
        shutdown();
        set_ship(ship.Get());
        set_camera(camera.Get());
        return false;
    }
    component_ = &component;
    subsystem_object_ = subsystem_object;
    initialised_ = true;
    global_.set_mapping_enabled(global_enabled);
    return set_control_context(previous_context);
}
void FPlayerControlContexts::shutdown() {
    set_control_context(EPlayerControlContext::None);
    benchmark_.shutdown();
    observer_.shutdown();
    ship_.shutdown();
    global_.shutdown();
    player_ship_.Reset();
    camera_.Reset();
    component_.Reset();
    subsystem_object_.Reset();
    initialised_ = false;
}
void FPlayerControlContexts::set_ship(ATestSpaceShip* const ship) {
    if ((!ship || player_ship_.Get() != ship) && active_context_ == EPlayerControlContext::Player) {
        set_control_context(EPlayerControlContext::None);
    }
    player_ship_ = ship;
    ship_.set_ship(ship);
}
void FPlayerControlContexts::set_camera(ACameraActor* const camera) {
    if ((!camera || camera_.Get() != camera) &&
        active_context_ == EPlayerControlContext::Observer) {
        set_control_context(EPlayerControlContext::None);
    }
    camera_ = camera;
    observer_.set_camera(camera);
}
auto FPlayerControlContexts::can_bind_context(EPlayerControlContext const context) const -> bool {
    switch (context) {
        case EPlayerControlContext::None: {
            return true;
        }
        case EPlayerControlContext::Player: {
            return ship_.can_bind();
        }
        case EPlayerControlContext::Observer: {
            return observer_.can_bind();
        }
        case EPlayerControlContext::Benchmark: {
            return benchmark_.can_bind();
        }
    }
    return false;
}
auto FPlayerControlContexts::bind_context(EPlayerControlContext const context) -> bool {
    bool bound{false};
    switch (context) {
        case EPlayerControlContext::None: {
            return true;
        }
        case EPlayerControlContext::Player: {
            global_.set_mapping_enabled(true);
            bound = ship_.bind();
            break;
        }
        case EPlayerControlContext::Observer: {
            global_.set_mapping_enabled(true);
            bound = observer_.bind();
            break;
        }
        case EPlayerControlContext::Benchmark: {
            global_.set_mapping_enabled(false);
            bound = benchmark_.bind();
            break;
        }
    }
#if WITH_DEV_AUTOMATION_TESTS
    // Fail after binding to exercise cleanup of partially acquired input resources.
    auto const failure_bit{static_cast<uint8>(1u << static_cast<uint8>(context))};
    if ((fail_bind_mask_ & failure_bit) != 0) {
        fail_bind_mask_ &= static_cast<uint8>(~failure_bit);
        return false;
    }
#endif
    return bound;
}
void FPlayerControlContexts::unbind_context(EPlayerControlContext const context) {
    switch (context) {
        case EPlayerControlContext::None: {
            break;
        }
        case EPlayerControlContext::Player: {
            ship_.unbind();
            break;
        }
        case EPlayerControlContext::Observer: {
            observer_.unbind();
            break;
        }
        case EPlayerControlContext::Benchmark: {
            benchmark_.unbind();
            break;
        }
    }
}
auto FPlayerControlContexts::set_control_context(EPlayerControlContext const context) -> bool {
    if (context == active_context_) {
        return bind_context(context);
    }
    if (!can_bind_context(context)) {
        UE_LOG(LogSandboxController,
               Error,
               TEXT("FPlayerControlContexts::set_control_context: Requested context cannot bind."));
        return false;
    }
    auto const previous_context{active_context_};
    auto const global_enabled{global_.is_mapping_enabled()};
    unbind_context(previous_context);
    active_context_ = EPlayerControlContext::None;
    if (bind_context(context)) {
        active_context_ = context;
        return true;
    }
    UE_LOG(LogSandboxController,
           Error,
           TEXT("FPlayerControlContexts::set_control_context: Failed to bind requested context."));
    unbind_context(context);
    if (bind_context(previous_context)) {
        active_context_ = previous_context;
    } else {
        unbind_context(previous_context);
        UE_LOG(LogSandboxController,
               Error,
               TEXT("FPlayerControlContexts::set_control_context: Failed to restore previous "
                    "context."));
    }
    global_.set_mapping_enabled(global_enabled);
    return false;
}
