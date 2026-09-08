#include <SpaceGame/ships/player/ObserverControlContext.h>

#include <SpaceGame/ships/player/SpaceGamePlayerController.h>
#include <SpaceGame/support/logging/SandboxLogCategories.h>

#include <Camera/CameraActor.h>
#include <Engine/World.h>
#include <EnhancedInputComponent.h>
#include <EnhancedInputSubsystemInterface.h>
#include <InputAction.h>
#include <InputActionValue.h>
#include <InputMappingContext.h>

auto FObserverControlContext::initialise(ASpaceGamePlayerController& owner,
                                         UEnhancedInputComponent& input_component,
                                         IEnhancedInputSubsystemInterface& input_subsystem,
                                         FObserverControlInputs const& input) -> bool {
    if (initialised_) {
        UE_LOG(LogSandboxController,
               Error,
               TEXT("FObserverControlContext::initialise: Context is already initialised."));
        return false;
    }

    auto* const input_subsystem_object{Cast<UObject>(&input_subsystem)};
    if (!IsValid(input_subsystem_object)) {
        UE_LOG(LogSandboxController,
               Error,
               TEXT("FObserverControlContext::initialise: Input subsystem is not a UObject."));
        return false;
    }

    owner_ = &owner;
    input_component_ = &input_component;
    input_subsystem_object_ = input_subsystem_object;
    input_subsystem_ = &input_subsystem;
    input_ = &input;
    initialised_ = true;
    return true;
}

auto FObserverControlContext::can_bind() const -> bool {
    return initialised_ && owner_.IsValid() && input_component_.IsValid() &&
           input_subsystem_object_.IsValid() && input_subsystem_ && camera_.IsValid() && input_ &&
           IsValid(input_->mapping_context) && IsValid(input_->move) &&
           IsValid(input_->vertical_move) && IsValid(input_->look) &&
           IsValid(input_->engage_look) && IsValid(input_->adjust_speed) && IsValid(input_->boost);
}

auto FObserverControlContext::bind() -> bool {
    if (bound_) {
        return true;
    }
    if (!can_bind()) {
        UE_LOG(LogSandboxController,
               Error,
               TEXT("FObserverControlContext::bind: Context dependencies are invalid."));
        return false;
    }

    bound_ = true;
    bind_actions();
    registered_mapping_ = input_->mapping_context;
    input_subsystem_->AddMappingContext(registered_mapping_.Get(), 0);
    publish_speed();
    return true;
}

void FObserverControlContext::unbind() {
    if (!bound_) {
        return;
    }

    end_look();
    boosting_ = false;
    if (input_subsystem_object_.IsValid() && input_subsystem_ && registered_mapping_.IsValid()) {
        input_subsystem_->RemoveMappingContext(registered_mapping_.Get());
    }
    registered_mapping_.Reset();
    remove_action_bindings();
    bound_ = false;
}

void FObserverControlContext::shutdown() {
    unbind();
    camera_.Reset();
    input_subsystem_object_.Reset();
    input_subsystem_ = nullptr;
    input_component_.Reset();
    owner_.Reset();
    input_ = nullptr;
    initialised_ = false;
}

void FObserverControlContext::set_camera(ACameraActor* const camera) {
    if (bound_ && camera_.Get() != camera) {
        unbind();
    }
    camera_ = camera;
}

auto FObserverControlContext::get_movement_speed() const noexcept -> float {
    auto const speed{movement_speeds_[speed_index_]};
    return boosting_ ? speed * boost_multiplier_ : speed;
}

void FObserverControlContext::bind_actions() {
    auto* const input_component{input_component_.Get()};
    check(IsValid(input_component) && input_);

    auto bind_value{[this, input_component](UInputAction* const action, auto method) {
        auto& binding{input_component->BindActionValueLambda(
            action, ETriggerEvent::Triggered, [this, method](FInputActionValue const& value) {
                if (bound_) {
                    (this->*method)(value);
                }
            })};
        binding_handles_.Add(binding.GetHandle());
    }};
    auto bind_event{[this, input_component](
                        UInputAction* const action, ETriggerEvent const event, auto method) {
        auto& binding{input_component->BindActionValueLambda(
            action, event, [this, method](FInputActionValue const&) {
                if (bound_) {
                    (this->*method)();
                }
            })};
        binding_handles_.Add(binding.GetHandle());
    }};

    bind_value(input_->move, &FObserverControlContext::move);
    bind_value(input_->vertical_move, &FObserverControlContext::move_vertical);
    bind_value(input_->look, &FObserverControlContext::look);
    bind_value(input_->adjust_speed, &FObserverControlContext::adjust_speed);
    bind_event(input_->engage_look, ETriggerEvent::Started, &FObserverControlContext::toggle_look);
    bind_event(input_->boost, ETriggerEvent::Started, &FObserverControlContext::begin_boost);
    bind_event(input_->boost, ETriggerEvent::Completed, &FObserverControlContext::end_boost);
}

void FObserverControlContext::remove_action_bindings() {
    if (auto* const input_component{input_component_.Get()}; IsValid(input_component)) {
        for (auto const handle : binding_handles_) {
            input_component->RemoveBindingByHandle(handle);
        }
    }
    binding_handles_.Reset();
}

void FObserverControlContext::toggle_look() {
    looking_ = !looking_;
    if (auto* const owner{owner_.Get()}) {
        owner->set_observer_look_active(looking_);
    }
}

void FObserverControlContext::end_look() {
    if (!looking_) {
        return;
    }
    looking_ = false;
    if (auto* const owner{owner_.Get()}) {
        owner->set_observer_look_active(false);
    }
}

void FObserverControlContext::move(FInputActionValue const& value) {
    auto* const camera{get_camera()};
    auto* const world{camera ? camera->GetWorld() : nullptr};
    if (!IsValid(world)) {
        return;
    }

    auto const input_value{value.Get<FVector2D>().GetClampedToMaxSize(1.0)};
    auto const offset{(camera->GetActorRightVector() * input_value.X +
                       camera->GetActorForwardVector() * input_value.Y) *
                      get_movement_speed() * world->GetDeltaSeconds()};
    camera->AddActorWorldOffset(offset);
}

void FObserverControlContext::move_vertical(FInputActionValue const& value) {
    auto* const camera{get_camera()};
    auto* const world{camera ? camera->GetWorld() : nullptr};
    if (!IsValid(world)) {
        return;
    }

    auto const input_value{FMath::Clamp(value.Get<float>(), -1.f, 1.f)};
    camera->AddActorWorldOffset(FVector::UpVector * input_value * get_movement_speed() *
                                world->GetDeltaSeconds());
}

void FObserverControlContext::look(FInputActionValue const& value) {
    auto* const camera{get_camera()};
    if (!looking_ || !camera) {
        return;
    }

    auto const input_value{value.Get<FVector2D>()};
    auto rotation{camera->GetActorRotation()};
    rotation.Yaw += input_value.X * look_sensitivity_;
    rotation.Pitch =
        FMath::ClampAngle(rotation.Pitch + input_value.Y * look_sensitivity_, -89.f, 89.f);
    rotation.Roll = 0.f;
    camera->SetActorRotation(rotation);
}

void FObserverControlContext::adjust_speed(FInputActionValue const& value) {
    auto const direction{FMath::Sign(value.Get<float>())};
    if (direction == 0.f) {
        return;
    }

    speed_index_ = FMath::Clamp(speed_index_ + static_cast<int32>(direction),
                                0,
                                static_cast<int32>(std::size(movement_speeds_)) - 1);
    publish_speed();
}

void FObserverControlContext::begin_boost() {
    boosting_ = true;
    publish_speed();
}

void FObserverControlContext::end_boost() {
    boosting_ = false;
    publish_speed();
}

void FObserverControlContext::publish_speed() const {
    if (auto* const owner{owner_.Get()}) {
        owner->set_observer_movement_speed(get_movement_speed());
    }
}

auto FObserverControlContext::get_camera() const -> ACameraActor* {
    auto* const camera{camera_.Get()};
    if (!IsValid(camera)) {
        UE_LOG(LogSandboxController,
               Error,
               TEXT("FObserverControlContext: Observer camera is invalid."));
        return nullptr;
    }
    return camera;
}
