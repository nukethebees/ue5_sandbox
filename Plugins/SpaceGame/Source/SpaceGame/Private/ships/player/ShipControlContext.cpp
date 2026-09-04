#include <SpaceGame/ships/player/ShipControlContext.h>

#include <SpaceGame/ships/player/SpaceGamePlayerController.h>
#include <SpaceGame/ships/player/TestSpaceShip.h>
#include <SpaceGame/support/logging/SandboxLogCategories.h>

#include <EnhancedInputComponent.h>
#include <EnhancedInputSubsystemInterface.h>
#include <EnhancedInputSubsystems.h>
#include <InputAction.h>
#include <InputActionValue.h>
#include <InputMappingContext.h>

auto FShipControlContext::initialise(ASpaceGamePlayerController& owner,
                                     UEnhancedInputComponent& input_component,
                                     IEnhancedInputSubsystemInterface& input_subsystem,
                                     FSpaceShipControllerInputs const& input) -> bool {
    if (initialised_) {
        UE_LOG(LogSandboxController,
               Error,
               TEXT("FShipControlContext::initialise: Context is already initialised."));
        return false;
    }

    auto* const input_subsystem_object{Cast<UObject>(&input_subsystem)};
    if (!IsValid(input_subsystem_object)) {
        UE_LOG(LogSandboxController,
               Error,
               TEXT("FShipControlContext::initialise: Input subsystem is not a UObject."));
        return false;
    }

    owner_ = &owner;
    input_component_ = &input_component;
    input_subsystem_object_ = input_subsystem_object;
    input_subsystem_ = &input_subsystem;
    input_ = &input;
    mapping_context_index_ = input.initial_mapping_context_index;
    initialised_ = true;
    return true;
}

auto FShipControlContext::can_bind() const -> bool {
    if (!initialised_ || !owner_.IsValid() || !input_component_.IsValid() ||
        !input_subsystem_object_.IsValid() || !input_subsystem_ || !ship_.IsValid() || !input_) {
        return false;
    }

    return input_->mapping_contexts.IsValidIndex(mapping_context_index_) &&
           IsValid(input_->mapping_contexts[mapping_context_index_]);
}

auto FShipControlContext::bind() -> bool {
    if (bound_) {
        return true;
    }

    if (!can_bind()) {
        UE_LOG(LogSandboxController,
               Error,
               TEXT("FShipControlContext::bind: Context dependencies are invalid."));
        return false;
    }

    bound_ = true;
    bind_actions();
    add_selected_mapping_context();
    return true;
}

void FShipControlContext::unbind() {
    if (!bound_) {
        return;
    }

    bound_ = false;
    neutralise_ship_input();
    remove_selected_mapping_context();
    remove_action_bindings();
}

void FShipControlContext::shutdown() {
    unbind();
    ship_.Reset();
    input_subsystem_object_.Reset();
    input_subsystem_ = nullptr;
    input_component_.Reset();
    owner_.Reset();
    input_ = nullptr;
    initialised_ = false;
}

void FShipControlContext::set_ship(ATestSpaceShip* const ship) {
    if (bound_ && ship_.Get() != ship) {
        unbind();
    }
    ship_ = ship;
}

void FShipControlContext::bind_actions() {
    auto* const input_component{input_component_.Get()};
    check(IsValid(input_component));
    check(input_);

    auto bind_value{[this, input_component](
                        UInputAction* const action, ETriggerEvent const event, auto method) {
        if (!IsValid(action)) {
            UE_LOG(LogSandboxController,
                   Warning,
                   TEXT("FShipControlContext::bind_actions: Input action is invalid."));
            return;
        }

        auto& binding{input_component->BindActionValueLambda(
            action, event, [this, method](FInputActionValue const& value) {
                if (bound_) {
                    (this->*method)(value);
                }
            })};
        binding_handles_.Add(binding.GetHandle());
    }};
    auto bind_no_value{[this, input_component](
                           UInputAction* const action, ETriggerEvent const event, auto method) {
        if (!IsValid(action)) {
            UE_LOG(LogSandboxController,
                   Warning,
                   TEXT("FShipControlContext::bind_actions: Input action is invalid."));
            return;
        }

        auto& binding{input_component->BindActionValueLambda(
            action, event, [this, method](FInputActionValue const&) {
                if (bound_) {
                    (this->*method)();
                }
            })};
        binding_handles_.Add(binding.GetHandle());
    }};

    using enum ETriggerEvent;

    bind_value(input_->move, Triggered, &FShipControlContext::set_move_input);
    bind_no_value(input_->move, Completed, &FShipControlContext::move_completed);
    bind_value(input_->lateral_move, Triggered, &FShipControlContext::set_lateral_move_input);
    bind_no_value(input_->lateral_move, Completed, &FShipControlContext::lateral_move_completed);
    bind_value(input_->vertical_move, Triggered, &FShipControlContext::set_vertical_move_input);
    bind_no_value(input_->vertical_move, Completed, &FShipControlContext::vertical_move_completed);

    bind_no_value(
        input_->ship_2d_control, Started, &FShipControlContext::set_ship_2d_control_started);
    bind_value(input_->ship_2d_control, Triggered, &FShipControlContext::set_ship_2d_control);
    bind_no_value(
        input_->ship_2d_control, Completed, &FShipControlContext::ship_2d_control_completed);
    bind_no_value(
        input_->ship_1d_control_x, Started, &FShipControlContext::set_ship_2d_control_started);
    bind_value(input_->ship_1d_control_x, Triggered, &FShipControlContext::set_ship_1d_control_x);
    bind_no_value(
        input_->ship_1d_control_x, Completed, &FShipControlContext::ship_2d_control_completed);
    bind_no_value(
        input_->ship_1d_control_y, Started, &FShipControlContext::set_ship_2d_control_started);
    bind_value(input_->ship_1d_control_y, Triggered, &FShipControlContext::set_ship_1d_control_y);
    bind_no_value(
        input_->ship_1d_control_y, Completed, &FShipControlContext::ship_2d_control_completed);

    bind_no_value(
        input_->cycle_next_control_mode, Started, &FShipControlContext::cycle_next_control_mode);
    bind_no_value(input_->cycle_previous_control_mode,
                  Started,
                  &FShipControlContext::cycle_previous_control_mode);
    bind_no_value(input_->sample_and_hold, Started, &FShipControlContext::start_sampling);
    bind_no_value(input_->sample_and_hold, Completed, &FShipControlContext::stop_sampling);

    bind_value(input_->turn, Triggered, &FShipControlContext::turn);
    bind_no_value(input_->turn, Completed, &FShipControlContext::turn_completed);
    bind_value(input_->roll, Started, &FShipControlContext::start_roll);
    bind_value(input_->roll, Triggered, &FShipControlContext::roll);
    bind_value(input_->roll, Completed, &FShipControlContext::stop_roll);
    bind_no_value(input_->boost, Started, &FShipControlContext::start_boost);
    bind_no_value(input_->boost, Completed, &FShipControlContext::stop_boost);
    bind_no_value(input_->brake, Started, &FShipControlContext::start_brake);
    bind_no_value(input_->brake, Completed, &FShipControlContext::stop_brake);

    bind_no_value(input_->fire_laser, Started, &FShipControlContext::start_fire_laser);
    bind_no_value(input_->fire_laser, Completed, &FShipControlContext::stop_fire_laser);
    bind_no_value(input_->fire_bomb, Started, &FShipControlContext::fire_bomb);
    bind_no_value(
        input_->cycle_prev_fire_rate, Started, &FShipControlContext::cycle_prev_fire_rate);
    bind_no_value(
        input_->cycle_next_fire_rate, Started, &FShipControlContext::cycle_next_fire_rate);
    bind_no_value(input_->cycle_input_mapping_context,
                  Started,
                  &FShipControlContext::cycle_input_mapping_context);
}

void FShipControlContext::remove_action_bindings() {
    auto* const input_component{input_component_.Get()};
    if (IsValid(input_component)) {
        for (uint32 const handle : binding_handles_) {
            input_component->RemoveBindingByHandle(handle);
        }
    }
    binding_handles_.Reset();
}

void FShipControlContext::add_selected_mapping_context() {
    check(input_subsystem_object_.IsValid() && input_subsystem_);
    check(input_ && input_->mapping_contexts.IsValidIndex(mapping_context_index_));

    auto* const context{input_->mapping_contexts[mapping_context_index_]};
    input_subsystem_->AddMappingContext(context, 0);

    if (auto* const owner{owner_.Get()}) {
        owner->on_ship_mapping_context_changed(*context);
    }
}

void FShipControlContext::remove_selected_mapping_context() {
    if (!input_subsystem_object_.IsValid() || !input_subsystem_ || !input_ ||
        !input_->mapping_contexts.IsValidIndex(mapping_context_index_)) {
        return;
    }

    input_subsystem_->RemoveMappingContext(input_->mapping_contexts[mapping_context_index_]);
}

void FShipControlContext::neutralise_ship_input() {
    auto* const ship{ship_.Get()};
    if (!IsValid(ship)) {
        return;
    }

    ship->set_move_input(FVector2D::ZeroVector);
    ship->set_lateral_move_input(0.f);
    ship->set_vertical_move_input(0.f);
    ship->set_ship_2d_control(FVector2D::ZeroVector);
    ship->set_ship_1d_control_x(0.f);
    ship->set_ship_1d_control_y(0.f);
    ship->turn(FVector2D::ZeroVector);
    ship->roll(0.f);
    ship->stop_sampling();
    ship->stop_boost();
    ship->stop_brake();
    ship->stop_fire_laser();
}

auto FShipControlContext::get_ship() const -> ATestSpaceShip* {
    auto* const ship{ship_.Get()};
    if (!IsValid(ship)) {
        UE_LOG(LogSandboxController, Error, TEXT("FShipControlContext: Player ship is invalid."));
        return nullptr;
    }
    return ship;
}

void FShipControlContext::set_move_input(FInputActionValue const& value) {
    if (auto* const ship{get_ship()}) {
        ship->set_move_input(value.Get<FVector2D>());
    }
}
void FShipControlContext::move_completed() {
    if (auto* const ship{get_ship()}) {
        ship->set_move_input(FVector2D::ZeroVector);
    }
}
void FShipControlContext::set_lateral_move_input(FInputActionValue const& value) {
    if (auto* const ship{get_ship()}) {
        ship->set_lateral_move_input(value.Get<float>());
    }
}
void FShipControlContext::lateral_move_completed() {
    if (auto* const ship{get_ship()}) {
        ship->set_lateral_move_input(0.f);
    }
}
void FShipControlContext::set_vertical_move_input(FInputActionValue const& value) {
    if (auto* const ship{get_ship()}) {
        ship->set_vertical_move_input(value.Get<float>());
    }
}
void FShipControlContext::vertical_move_completed() {
    if (auto* const ship{get_ship()}) {
        ship->set_vertical_move_input(0.f);
    }
}
void FShipControlContext::set_ship_2d_control_started() {
    start_sampling();
}
void FShipControlContext::set_ship_2d_control(FInputActionValue const& value) {
    if (auto* const ship{get_ship()}) {
        ship->set_ship_2d_control(value.Get<FVector2D>());
    }
}
void FShipControlContext::ship_2d_control_completed() {
    stop_sampling();
}
void FShipControlContext::set_ship_1d_control_x(FInputActionValue const& value) {
    if (auto* const ship{get_ship()}) {
        ship->set_ship_1d_control_x(value.Get<float>());
    }
}
void FShipControlContext::set_ship_1d_control_y(FInputActionValue const& value) {
    if (auto* const ship{get_ship()}) {
        ship->set_ship_1d_control_y(value.Get<float>());
    }
}
void FShipControlContext::cycle_next_control_mode() {
    if (auto* const ship{get_ship()}) {
        ship->select_next_control_mode();
    }
}
void FShipControlContext::cycle_previous_control_mode() {
    if (auto* const ship{get_ship()}) {
        ship->select_previous_control_mode();
    }
}
void FShipControlContext::start_sampling() {
    if (auto* const ship{get_ship()}) {
        ship->start_sampling();
    }
}
void FShipControlContext::stop_sampling() {
    if (auto* const ship{get_ship()}) {
        ship->stop_sampling();
    }
}
void FShipControlContext::turn(FInputActionValue const& value) {
    if (auto* const ship{get_ship()}) {
        ship->turn(value.Get<FVector2D>());
    }
}
void FShipControlContext::turn_completed() {
    if (auto* const ship{get_ship()}) {
        ship->turn(FVector2D::ZeroVector);
    }
}
void FShipControlContext::start_roll(FInputActionValue const& value) {
    UE_LOG(LogSandboxController, Verbose, TEXT("Begin roll: %.1f"), value.Get<float>());
}
void FShipControlContext::roll(FInputActionValue const& value) {
    if (auto* const ship{get_ship()}) {
        ship->roll(FMath::Clamp(value.Get<float>(), -1.f, 1.f));
    }
}
void FShipControlContext::stop_roll(FInputActionValue const& value) {
    UE_LOG(LogSandboxController, Verbose, TEXT("End roll: %.1f"), value.Get<float>());
    if (auto* const ship{get_ship()}) {
        ship->roll(0.f);
    }
}
void FShipControlContext::start_boost() {
    if (auto* const ship{get_ship()}) {
        ship->start_boost();
    }
}
void FShipControlContext::stop_boost() {
    if (auto* const ship{get_ship()}) {
        ship->stop_boost();
    }
}
void FShipControlContext::start_brake() {
    if (auto* const ship{get_ship()}) {
        ship->start_brake();
    }
}
void FShipControlContext::stop_brake() {
    if (auto* const ship{get_ship()}) {
        ship->stop_brake();
    }
}
void FShipControlContext::start_fire_laser() {
    if (auto* const ship{get_ship()}) {
        ship->start_fire_laser();
    }
}
void FShipControlContext::stop_fire_laser() {
    if (auto* const ship{get_ship()}) {
        ship->stop_fire_laser();
    }
}
void FShipControlContext::fire_bomb() {
    if (auto* const ship{get_ship()}) {
        ship->fire_bomb();
    }
}
void FShipControlContext::cycle_prev_fire_rate() {
    if (auto* const ship{get_ship()}) {
        ship->select_previous_laser_fire_rate();
    }
}
void FShipControlContext::cycle_next_fire_rate() {
    if (auto* const ship{get_ship()}) {
        ship->select_next_laser_fire_rate();
    }
}

void FShipControlContext::cycle_input_mapping_context() {
    if (!bound_ || !input_) {
        return;
    }

    auto const n_contexts{input_->mapping_contexts.Num()};
    if (n_contexts <= 0 || !input_->mapping_contexts.IsValidIndex(mapping_context_index_)) {
        UE_LOG(LogSandboxController,
               Error,
               TEXT("FShipControlContext::cycle_input_mapping_context: Mapping index is invalid."));
        return;
    }

    auto const next_context_index{(mapping_context_index_ + 1) % n_contexts};
    if (!IsValid(input_->mapping_contexts[next_context_index])) {
        UE_LOG(LogSandboxController,
               Error,
               TEXT("FShipControlContext::cycle_input_mapping_context: Next mapping context is "
                    "invalid."));
        return;
    }

    remove_selected_mapping_context();
    mapping_context_index_ = next_context_index;
    add_selected_mapping_context();
}
