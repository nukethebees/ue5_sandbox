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
void FShipControlContext::bind() {
    if (bound_) {
        verify_mode_invariant();
        return;
    }
    check(can_bind());

    auto* const mapping{context_for_slot(ship_->get_active_flight_model_slot())};
    check(IsValid(mapping));

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
        case ::ioj::sim::player::FlightModelSlot::First:
            return input_->starfox;
        case ::ioj::sim::player::FlightModelSlot::Second:
            return input_->fighter;
        case ::ioj::sim::player::FlightModelSlot::Third:
            return input_->skater;
        case ::ioj::sim::player::FlightModelSlot::Fourth:
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
void FShipControlContext::bind_actions() {
    auto* const component{input_component_.Get()};
    check(IsValid(component) && input_);
    auto const bind_value{
        [this, component](UInputAction* const action, ETriggerEvent const event, auto callback) {
            check(IsValid(action));
            auto& binding{component->BindActionValueLambda(
                action, event, [this, callback](FInputActionValue const& value) {
                    if (bound_) {
                        callback(value);
                    }
                })};
            binding_handles_.Add(binding.GetHandle());
        }};
    auto const bind_event =
        [&bind_value](UInputAction* const action, ETriggerEvent const event, auto callback) {
            bind_value(action, event, [callback](FInputActionValue const&) { callback(); });
        };
    auto const ship_value = [this](auto method) {
        return [this, method](FInputActionValue const& value) {
            if (auto* const ship{get_ship()}) {
                (ship->*method)(value.Get<float>());
            }
        };
    };
    auto const ship_event = [this](auto method) {
        return [this, method] {
            if (auto* const ship{get_ship()}) {
                (ship->*method)();
            }
        };
    };
    using enum ETriggerEvent;
    auto const bind_release = [&bind_event](UInputAction* const action, auto callback) {
        bind_event(action, Completed, callback);
        bind_event(action, Canceled, callback);
    };
    auto const clear_axis = [this, &bind_release](UInputAction* const action, auto method) {
        bind_release(action, [this, method] {
            if (auto* const ship{get_ship()}) {
                (ship->*method)(0.f);
            }
        });
    };
    auto const select_slot = [this, &bind_event](UInputAction* const action,
                                                 ioj::sim::player::FlightModelSlot const slot) {
        bind_event(action, Started, [this, slot] { select_flight_model_slot(slot); });
    };

    bind_value(
        input_->translate_forward, Triggered, ship_value(&ATestSpaceShip::set_forward_input));
    bind_value(input_->translate_right, Triggered, ship_value(&ATestSpaceShip::set_right_input));
    bind_value(input_->translate_up, Triggered, [this](FInputActionValue const& value) {
        set_up_input(value);
    });
    bind_value(input_->pitch, Triggered, [this](FInputActionValue const& value) {
        set_pitch_input(value);
    });
    bind_value(input_->yaw, Triggered, ship_value(&ATestSpaceShip::set_yaw_input));
    bind_value(input_->roll, Triggered, ship_value(&ATestSpaceShip::set_roll_input));
    bind_value(input_->accelerate, Triggered, ship_value(&ATestSpaceShip::set_accelerator));
    clear_axis(input_->translate_forward, &ATestSpaceShip::set_forward_input);
    clear_axis(input_->translate_right, &ATestSpaceShip::set_right_input);
    clear_axis(input_->translate_up, &ATestSpaceShip::set_up_input);
    clear_axis(input_->pitch, &ATestSpaceShip::set_pitch_input);
    clear_axis(input_->yaw, &ATestSpaceShip::set_yaw_input);
    clear_axis(input_->roll, &ATestSpaceShip::set_roll_input);
    clear_axis(input_->accelerate, &ATestSpaceShip::set_accelerator);
    bind_event(input_->boost, Started, ship_event(&ATestSpaceShip::start_boost));
    bind_release(input_->boost, ship_event(&ATestSpaceShip::stop_boost));
    bind_event(input_->brake, Started, ship_event(&ATestSpaceShip::start_brake));
    bind_release(input_->brake, ship_event(&ATestSpaceShip::stop_brake));
    bind_event(
        input_->emergency_brake, Started, ship_event(&ATestSpaceShip::start_emergency_brake));
    bind_release(input_->emergency_brake, ship_event(&ATestSpaceShip::stop_emergency_brake));
    bind_event(input_->fire_primary, Started, [this] { start_fire_primary(); });
    bind_release(input_->fire_primary, ship_event(&ATestSpaceShip::stop_fire_laser));
    using enum ioj::sim::player::FlightModelSlot;
    select_slot(input_->select_starfox, First);
    select_slot(input_->select_fighter, Second);
    select_slot(input_->select_skater, Third);
    select_slot(input_->select_gunship, Fourth);
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
