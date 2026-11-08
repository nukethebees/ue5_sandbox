#include <SpaceGame/ships/player/PlayerControlContexts.h>

#include <SpaceGame/ships/player/SpaceGamePlayerController.h>
#include <SpaceGame/ships/player/TestSpaceShip.h>
#include <SpaceGameSimulation/support/logging/SandboxLogCategories.h>

#include <Camera/CameraActor.h>
#include <EnhancedInputComponent.h>
#include <EnhancedInputSubsystemInterface.h>

namespace ioj {
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
    set_control_context(levels::ControlContext::None);
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
    if ((!ship || player_ship_.Get() != ship) &&
        active_context_ == levels::ControlContext::Player) {
        set_control_context(levels::ControlContext::None);
    }
    player_ship_ = ship;
    ship_.set_ship(ship);
}
void FPlayerControlContexts::set_camera(ACameraActor* const camera) {
    if ((!camera || camera_.Get() != camera) &&
        active_context_ == levels::ControlContext::Observer) {
        set_control_context(levels::ControlContext::None);
    }
    camera_ = camera;
    observer_.set_camera(camera);
}
auto FPlayerControlContexts::can_bind_context(levels::ControlContext const context) const -> bool {
    switch (context) {
        case levels::ControlContext::None: {
            return true;
        }
        case levels::ControlContext::Player: {
            return ship_.can_bind();
        }
        case levels::ControlContext::Observer: {
            return observer_.can_bind();
        }
        case levels::ControlContext::Benchmark: {
            return benchmark_.can_bind();
        }
    }
    return false;
}
void FPlayerControlContexts::bind_context(levels::ControlContext const context) {
    switch (context) {
        case levels::ControlContext::None: {
            return;
        }
        case levels::ControlContext::Player: {
            global_.set_mapping_enabled(true);
            ship_.bind();
            break;
        }
        case levels::ControlContext::Observer: {
            global_.set_mapping_enabled(true);
            observer_.bind();
            break;
        }
        case levels::ControlContext::Benchmark: {
            global_.set_mapping_enabled(false);
            benchmark_.bind();
            break;
        }
    }
}
void FPlayerControlContexts::unbind_context(levels::ControlContext const context) {
    switch (context) {
        case levels::ControlContext::None: {
            break;
        }
        case levels::ControlContext::Player: {
            ship_.unbind();
            break;
        }
        case levels::ControlContext::Observer: {
            observer_.unbind();
            break;
        }
        case levels::ControlContext::Benchmark: {
            benchmark_.unbind();
            break;
        }
    }
}
auto FPlayerControlContexts::set_control_context(levels::ControlContext const context) -> bool {
    if (!can_bind_context(context)) {
        UE_LOG(LogSandboxController,
               Error,
               TEXT("FPlayerControlContexts::set_control_context: Requested context cannot bind."));
        return false;
    }
    if (context == active_context_) {
        return true;
    }

    unbind_context(active_context_);
    bind_context(context);
    active_context_ = context;
    return true;
}

} // namespace ioj
