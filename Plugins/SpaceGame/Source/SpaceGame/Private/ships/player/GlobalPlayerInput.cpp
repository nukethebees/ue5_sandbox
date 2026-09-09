#include <SpaceGame/ships/player/GlobalPlayerInput.h>

#include <SpaceGame/ships/player/SpaceGamePlayerController.h>
#include <SpaceGameSimulation/support/logging/SandboxLogCategories.h>

#include <EnhancedInputComponent.h>
#include <EnhancedInputSubsystemInterface.h>
#include <InputAction.h>
#include <InputMappingContext.h>

auto FGlobalPlayerInput::initialise(ASpaceGamePlayerController& owner,
                                    UEnhancedInputComponent& component,
                                    IEnhancedInputSubsystemInterface& subsystem,
                                    FGlobalControlInputs const& input) -> bool {
    if (bound_ && component_.Get() == &component && subsystem_ == &subsystem) {
        return true;
    }
    shutdown();
    auto* const subsystem_object{Cast<UObject>(&subsystem)};
    if (!IsValid(subsystem_object) || !IsValid(input.mapping_context) ||
        !IsValid(input.toggle_menu)) {
        UE_LOG(LogSandboxController,
               Error,
               TEXT("FGlobalPlayerInput::initialise: Input dependencies are invalid."));
        return false;
    }
    component_ = &component;
    subsystem_object_ = subsystem_object;
    subsystem_ = &subsystem;
    mapping_ = input.mapping_context;
    binding_handle_ = component
                          .BindAction(input.toggle_menu,
                                      ETriggerEvent::Started,
                                      &owner,
                                      &ASpaceGamePlayerController::toggle_pause_game)
                          .GetHandle();
    bound_ = true;
    set_mapping_enabled(true);
    return true;
}
void FGlobalPlayerInput::set_mapping_enabled(bool const enabled) {
    if (!bound_ || mapping_enabled_ == enabled) {
        return;
    }
    if (subsystem_object_.IsValid() && mapping_.IsValid()) {
        if (enabled) {
            subsystem_->AddMappingContext(mapping_.Get(), mapping_priority_);
        } else {
            subsystem_->RemoveMappingContext(mapping_.Get());
        }
    } else {
        UE_LOG(LogSandboxController,
               Warning,
               TEXT("FGlobalPlayerInput::set_mapping_enabled: Input dependencies have expired."));
    }
    mapping_enabled_ = enabled && subsystem_object_.IsValid() && mapping_.IsValid();
}
void FGlobalPlayerInput::shutdown() {
    set_mapping_enabled(false);
    if (bound_ && component_.IsValid()) {
        component_->RemoveBindingByHandle(binding_handle_);
    }
    binding_handle_ = 0;
    bound_ = false;
    mapping_enabled_ = false;
    mapping_.Reset();
    component_.Reset();
    subsystem_object_.Reset();
    subsystem_ = nullptr;
}
