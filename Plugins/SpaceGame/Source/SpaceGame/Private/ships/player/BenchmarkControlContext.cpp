#include <SpaceGame/ships/player/BenchmarkControlContext.h>

#include <SpaceGame/ships/player/SpaceGamePlayerController.h>
#include <SpaceGameSimulation/support/logging/SandboxLogCategories.h>

#include <EnhancedInputComponent.h>
#include <EnhancedInputSubsystemInterface.h>
#include <InputAction.h>
#include <InputMappingContext.h>

auto FBenchmarkControlContext::initialise(ASpaceGamePlayerController& owner,
                                          UEnhancedInputComponent& component,
                                          IEnhancedInputSubsystemInterface& subsystem,
                                          FBenchmarkControlInputs const& input) -> bool {
    if (owner_.Get() == &owner && component_.Get() == &component && subsystem_ == &subsystem &&
        input_ == &input) {
        return true;
    }
    shutdown();
    auto* const subsystem_object{Cast<UObject>(&subsystem)};
    if (!IsValid(subsystem_object)) {
        UE_LOG(LogSandboxController,
               Error,
               TEXT("FBenchmarkControlContext::initialise: Input subsystem is not a UObject."));
        return false;
    }
    owner_ = &owner;
    component_ = &component;
    subsystem_object_ = subsystem_object;
    subsystem_ = &subsystem;
    input_ = &input;
    return true;
}
auto FBenchmarkControlContext::can_bind() const -> bool {
    return owner_.IsValid() && component_.IsValid() && subsystem_object_.IsValid() && input_ &&
           input_->is_valid();
}
auto FBenchmarkControlContext::bind() -> bool {
    if (bound_) {
        return true;
    }
    if (!can_bind()) {
        UE_LOG(LogSandboxController,
               Error,
               TEXT("FBenchmarkControlContext::bind: Input dependencies are invalid."));
        return false;
    }
    binding_handle_ = component_
                          ->BindAction(input_->exit,
                                       ETriggerEvent::Started,
                                       owner_.Get(),
                                       &ASpaceGamePlayerController::return_to_level_select)
                          .GetHandle();
    registered_mapping_ = input_->mapping_context;
    subsystem_->AddMappingContext(registered_mapping_.Get(), mapping_priority_);
    bound_ = true;
    return true;
}
void FBenchmarkControlContext::unbind() {
    if (!bound_) {
        return;
    }
    if (subsystem_object_.IsValid() && registered_mapping_.IsValid()) {
        subsystem_->RemoveMappingContext(registered_mapping_.Get());
    }
    if (component_.IsValid()) {
        component_->RemoveBindingByHandle(binding_handle_);
    }
    registered_mapping_.Reset();
    binding_handle_ = 0;
    bound_ = false;
}
void FBenchmarkControlContext::shutdown() {
    unbind();
    owner_.Reset();
    component_.Reset();
    subsystem_object_.Reset();
    subsystem_ = nullptr;
    input_ = nullptr;
}
