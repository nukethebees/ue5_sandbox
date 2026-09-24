#include <SpaceGame/ships/player/ShipControlContext.h>

#include <SpaceGame/ships/player/SpaceGamePlayerController.h>
#include <SpaceGame/ships/player/TestSpaceShip.h>
#include <SpaceGameSimulation/support/logging/SandboxLogCategories.h>

#include <EnhancedInputComponent.h>
#include <EnhancedInputSubsystemInterface.h>
#include <HAL/IConsoleManager.h>
#include <InputAction.h>
#include <InputActionValue.h>
#include <InputMappingContext.h>

namespace spacegame::input_trace {
auto is_enabled() -> bool {
    auto const* const variable{
        IConsoleManager::Get().FindConsoleVariable(TEXT("spacegame.InputTrace"))};
    return variable != nullptr && variable->GetInt() != 0;
}
}

auto FShipControlContext::initialise(ASpaceGamePlayerController& owner,
                                     UEnhancedInputComponent& component,
                                     IEnhancedInputSubsystemInterface& subsystem,
                                     FSpaceShipControllerInputs const& input) -> bool {
    if (initialised_) {
        return false;
    }
    auto* const subsystem_object{Cast<UObject>(&subsystem)};
    if (!IsValid(subsystem_object)) {
        UE_LOG(LogSandboxController, Error, TEXT("Ship input subsystem is invalid."));
        return false;
    }

    owner_ = &owner;
    input_component_ = &component;
    input_subsystem_object_ = subsystem_object;
    input_subsystem_ = &subsystem;
    input_ = &input;
    initialised_ = true;
    return true;
}
auto FShipControlContext::can_bind() const -> bool {
    auto* const ship{ship_.Get()};
    if (!initialised_ || !owner_.IsValid() || !input_component_.IsValid() ||
        !input_subsystem_object_.IsValid() || !input_subsystem_ || !IsValid(ship) ||
        !ship->has_simulation() || !input_) {
        return false;
    }
    return IsValid(input_->starfox) && IsValid(input_->fighter) && IsValid(input_->skater) &&
           IsValid(input_->gunship);
}
auto FShipControlContext::bind() -> bool {
    if (bound_) {
        verify_mode_invariant();
        return true;
    }
    if (!can_bind()) {
        UE_LOG(LogSandboxController, Error, TEXT("Ship input cannot bind."));
        return false;
    }

    auto* const mapping{context_for_slot(ship_->get_active_flight_model_slot())};
    if (!IsValid(mapping)) {
        UE_LOG(LogSandboxController, Error, TEXT("Selected flight mode IMC is invalid."));
        return false;
    }

    bound_ = true;
    bind_actions();
    FModifyContextOptions options{};
    options.bForceImmediately = true;
    input_subsystem_->AddMappingContext(mapping, 0, options);
    active_mode_mapping_ = mapping;
    verify_mode_invariant();
    if (auto* const owner{owner_.Get()}) {
        owner->on_player_ship_flight_model_selected();
    }
    return true;
}
void FShipControlContext::unbind() {
    if (!bound_) {
        return;
    }
    bound_ = false;
    neutralise_ship_input();
    if (input_subsystem_object_.IsValid() && input_subsystem_ && active_mode_mapping_.IsValid()) {
        input_subsystem_->RemoveMappingContext(active_mode_mapping_.Get());
    }
    active_mode_mapping_.Reset();
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
auto FShipControlContext::context_for_slot(::ioj::sim::player::FlightModelSlot const slot) const
    -> UInputMappingContext* {
    if (!input_) {
        return nullptr;
    }
    switch (slot) {
        case ::ioj::sim::player::FlightModelSlot::Up:
            return input_->starfox;
        case ::ioj::sim::player::FlightModelSlot::Right:
            return input_->fighter;
        case ::ioj::sim::player::FlightModelSlot::Down:
            return input_->skater;
        case ::ioj::sim::player::FlightModelSlot::Left:
            return input_->gunship;
    }
    return nullptr;
}
void FShipControlContext::verify_mode_invariant() const {
    if (!bound_ || !input_subsystem_object_.IsValid() || !input_subsystem_) {
        return;
    }
    int32 count{};
    for (auto* const mapping :
         {input_->starfox, input_->fighter, input_->skater, input_->gunship}) {
        count += IsValid(mapping) && input_subsystem_->HasMappingContext(mapping) ? 1 : 0;
    }
    checkf(count == 1 && input_subsystem_->HasMappingContext(active_mode_mapping_.Get()),
           TEXT("Ship input must have exactly one flight-mode IMC."));
}
void FShipControlContext::select_flight_model_slot(::ioj::sim::player::FlightModelSlot const slot) {
    auto* const ship{get_ship()};
    auto* const next{context_for_slot(slot)};
    if (!bound_ || !IsValid(ship) || !IsValid(next) || !input_subsystem_object_.IsValid()) {
        return;
    }
    if (ship->get_active_flight_model_slot() == slot) {
        verify_mode_invariant();
        return;
    }

    auto* const previous{active_mode_mapping_.Get()};
    neutralise_ship_input();
    input_subsystem_->RemoveMappingContext(previous);
    ship->select_flight_model_slot(slot);
    FModifyContextOptions options{};
    options.bForceImmediately = true;
    input_subsystem_->AddMappingContext(next, 0, options);
    active_mode_mapping_ = next;
    verify_mode_invariant();
    if (auto* const owner{owner_.Get()}) {
        owner->on_player_ship_flight_model_selected();
    }
}
void FShipControlContext::select_starfox() {
    select_flight_model_slot(::ioj::sim::player::FlightModelSlot::Up);
}
void FShipControlContext::select_fighter() {
    select_flight_model_slot(::ioj::sim::player::FlightModelSlot::Right);
}
void FShipControlContext::select_skater() {
    select_flight_model_slot(::ioj::sim::player::FlightModelSlot::Down);
}
void FShipControlContext::select_gunship() {
    select_flight_model_slot(::ioj::sim::player::FlightModelSlot::Left);
}
void FShipControlContext::bind_actions() {
    auto* const component{input_component_.Get()};
    check(IsValid(component) && input_);
    auto bind_value{
        [this, component](UInputAction* const action, ETriggerEvent const event, auto method) {
            check(IsValid(action));
            auto& binding{component->BindActionValueLambda(
                action, event, [this, method](FInputActionValue const& value) {
                    if (bound_) {
                        (this->*method)(value);
                    }
                })};
            binding_handles_.Add(binding.GetHandle());
        }};
    auto bind_event{
        [this, component](UInputAction* const action, ETriggerEvent const event, auto method) {
            check(IsValid(action));
            auto& binding{component->BindActionValueLambda(
                action, event, [this, method](FInputActionValue const&) {
                    if (bound_) {
                        (this->*method)();
                    }
                })};
            binding_handles_.Add(binding.GetHandle());
        }};
    using enum ETriggerEvent;
    bind_value(input_->translate_forward, Triggered, &FShipControlContext::set_forward_input);
    bind_value(input_->translate_right, Triggered, &FShipControlContext::set_right_input);
    bind_value(input_->translate_up, Triggered, &FShipControlContext::set_up_input);
    bind_value(input_->pitch, Triggered, &FShipControlContext::set_pitch_input);
    bind_value(input_->yaw, Triggered, &FShipControlContext::set_yaw_input);
    bind_value(input_->roll, Triggered, &FShipControlContext::set_roll_input);
    bind_value(input_->accelerate, Triggered, &FShipControlContext::set_accelerator);
    bind_event(input_->translate_forward, Completed, &FShipControlContext::clear_forward_input);
    bind_event(input_->translate_forward, Canceled, &FShipControlContext::clear_forward_input);
    bind_event(input_->translate_right, Completed, &FShipControlContext::clear_right_input);
    bind_event(input_->translate_right, Canceled, &FShipControlContext::clear_right_input);
    bind_event(input_->translate_up, Completed, &FShipControlContext::clear_up_input);
    bind_event(input_->translate_up, Canceled, &FShipControlContext::clear_up_input);
    bind_event(input_->pitch, Completed, &FShipControlContext::clear_pitch_input);
    bind_event(input_->pitch, Canceled, &FShipControlContext::clear_pitch_input);
    bind_event(input_->yaw, Completed, &FShipControlContext::clear_yaw_input);
    bind_event(input_->yaw, Canceled, &FShipControlContext::clear_yaw_input);
    bind_event(input_->roll, Completed, &FShipControlContext::clear_roll_input);
    bind_event(input_->roll, Canceled, &FShipControlContext::clear_roll_input);
    bind_event(input_->accelerate, Completed, &FShipControlContext::clear_accelerator);
    bind_event(input_->accelerate, Canceled, &FShipControlContext::clear_accelerator);
    bind_event(input_->boost, Started, &FShipControlContext::start_boost);
    bind_event(input_->boost, Completed, &FShipControlContext::stop_boost);
    bind_event(input_->boost, Canceled, &FShipControlContext::stop_boost);
    bind_event(input_->brake, Started, &FShipControlContext::start_brake);
    bind_event(input_->brake, Completed, &FShipControlContext::stop_brake);
    bind_event(input_->brake, Canceled, &FShipControlContext::stop_brake);
    bind_event(input_->emergency_brake, Started, &FShipControlContext::start_emergency_brake);
    bind_event(input_->emergency_brake, Completed, &FShipControlContext::stop_emergency_brake);
    bind_event(input_->emergency_brake, Canceled, &FShipControlContext::stop_emergency_brake);
    bind_event(input_->fire_primary, Started, &FShipControlContext::start_fire_primary);
    bind_event(input_->fire_primary, Completed, &FShipControlContext::stop_fire_primary);
    bind_event(input_->fire_primary, Canceled, &FShipControlContext::stop_fire_primary);
    bind_event(input_->select_starfox, Started, &FShipControlContext::select_starfox);
    bind_event(input_->select_fighter, Started, &FShipControlContext::select_fighter);
    bind_event(input_->select_skater, Started, &FShipControlContext::select_skater);
    bind_event(input_->select_gunship, Started, &FShipControlContext::select_gunship);
}
void FShipControlContext::remove_action_bindings() {
    if (auto* const component{input_component_.Get()}) {
        for (auto const handle : binding_handles_) {
            component->RemoveBindingByHandle(handle);
        }
    }
    binding_handles_.Reset();
}
void FShipControlContext::neutralise_ship_input() {
    auto* const ship{ship_.Get()};
    if (!IsValid(ship) || !ship->has_simulation()) {
        return;
    }
    ship->set_forward_input(0.f);
    ship->set_right_input(0.f);
    ship->set_up_input(0.f);
    ship->set_pitch_input(0.f);
    ship->set_yaw_input(0.f);
    ship->set_roll_input(0.f);
    ship->set_accelerator(0.f);
    ship->stop_boost();
    ship->stop_brake();
    ship->stop_emergency_brake();
    ship->stop_fire_laser();
}
auto FShipControlContext::get_ship() const -> ATestSpaceShip* {
    auto* const ship{ship_.Get()};
    if (!IsValid(ship) || !ship->has_simulation()) {
        UE_LOG(LogSandboxController, Warning, TEXT("Player ship simulation is unavailable."));
        return nullptr;
    }
    return ship;
}
void FShipControlContext::set_forward_input(FInputActionValue const& value) {
    if (auto* const ship{get_ship()}) {
        ship->set_forward_input(value.Get<float>());
    }
}
void FShipControlContext::set_right_input(FInputActionValue const& value) {
    if (auto* const ship{get_ship()}) {
        ship->set_right_input(value.Get<float>());
    }
}
void FShipControlContext::set_up_input(FInputActionValue const& value) {
    if (auto* const ship{get_ship()}) {
        if (spacegame::input_trace::is_enabled()) {
            UE_LOG(LogSandboxController,
                   Warning,
                   TEXT("[InputTrace] IA_Ship_TranslateUp -> native vertical intent %.3f"),
                   value.Get<float>());
        }
        ship->set_up_input(value.Get<float>());
    }
}
void FShipControlContext::set_pitch_input(FInputActionValue const& value) {
    if (auto* const ship{get_ship()}) {
        if (spacegame::input_trace::is_enabled()) {
            UE_LOG(LogSandboxController,
                   Warning,
                   TEXT("[InputTrace] IA_Ship_Pitch -> native pitch intent %.3f"),
                   value.Get<float>());
        }
        ship->set_pitch_input(value.Get<float>());
    }
}
void FShipControlContext::set_yaw_input(FInputActionValue const& value) {
    if (auto* const ship{get_ship()}) {
        ship->set_yaw_input(value.Get<float>());
    }
}
void FShipControlContext::set_roll_input(FInputActionValue const& value) {
    if (auto* const ship{get_ship()}) {
        ship->set_roll_input(value.Get<float>());
    }
}
void FShipControlContext::set_accelerator(FInputActionValue const& value) {
    if (auto* const ship{get_ship()}) {
        ship->set_accelerator(value.Get<float>());
    }
}
void FShipControlContext::clear_forward_input() {
    if (auto* const ship{get_ship()}) {
        ship->set_forward_input(0.f);
    }
}
void FShipControlContext::clear_right_input() {
    if (auto* const ship{get_ship()}) {
        ship->set_right_input(0.f);
    }
}
void FShipControlContext::clear_up_input() {
    if (auto* const ship{get_ship()}) {
        ship->set_up_input(0.f);
    }
}
void FShipControlContext::clear_pitch_input() {
    if (auto* const ship{get_ship()}) {
        ship->set_pitch_input(0.f);
    }
}
void FShipControlContext::clear_yaw_input() {
    if (auto* const ship{get_ship()}) {
        ship->set_yaw_input(0.f);
    }
}
void FShipControlContext::clear_roll_input() {
    if (auto* const ship{get_ship()}) {
        ship->set_roll_input(0.f);
    }
}
void FShipControlContext::clear_accelerator() {
    if (auto* const ship{get_ship()}) {
        ship->set_accelerator(0.f);
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
void FShipControlContext::start_emergency_brake() {
    if (auto* const ship{get_ship()}) {
        ship->start_emergency_brake();
    }
}
void FShipControlContext::stop_emergency_brake() {
    if (auto* const ship{get_ship()}) {
        ship->stop_emergency_brake();
    }
}
void FShipControlContext::start_fire_primary() {
    if (auto* const ship{get_ship()}) {
        if (spacegame::input_trace::is_enabled()) {
            UE_LOG(LogSandboxController,
                   Warning,
                   TEXT("[InputTrace] IA_Ship_FirePrimary -> start_fire_laser"));
        }
        ship->start_fire_laser();
    }
}
void FShipControlContext::stop_fire_primary() {
    if (auto* const ship{get_ship()}) {
        ship->stop_fire_laser();
    }
}
