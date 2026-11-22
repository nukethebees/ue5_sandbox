#include <SpaceGame/ships/player/ShipControlContext.h>

#include <SpaceGame/settings/GameSettingsSubsystem.h>
#include <SpaceGame/ships/player/SpaceGamePlayerController.h>
#include <SpaceGame/ships/player/TestSpaceShip.h>
#include <SpaceGameSimulation/support/logging/SandboxLogCategories.h>

#include <Engine/GameInstance.h>
#include <EnhancedInputComponent.h>
#include <EnhancedInputSubsystemInterface.h>
#include <EnhancedPlayerInput.h>
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

namespace ioj {
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
    return IsValid(input_->starfox);
}
void FShipControlContext::bind() {
    if (bound_) {
        return;
    }
    check(can_bind());

    auto* const mapping{input_->starfox};
    check(IsValid(mapping));

    bound_ = true;
    bind_actions();
    FModifyContextOptions options{};
    options.bForceImmediately = true;
    input_subsystem_->AddMappingContext(mapping, 0, options);
}
void FShipControlContext::unbind() {
    if (!bound_) {
        return;
    }
    bound_ = false;
    neutralise_ship_input();
    if (input_subsystem_object_.IsValid() && input_subsystem_) {
        input_subsystem_->RemoveMappingContext(input_->starfox);
    }
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
    auto* const component{input_component_.Get()};
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
                (ship->commands().*method)(value.Get<float>());
            }
        };
    };
    auto const ship_event = [this](auto method) {
        return [this, method] {
            if (auto* const ship{get_ship()}) {
                (ship->commands().*method)();
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
                (ship->commands().*method)(0.f);
            }
        });
    };
    for (auto* const axis : {input_->pitch, input_->yaw}) {
        auto const turn{[this] { update_gamepad_turn(); }};
        bind_event(axis, Triggered, turn);
        bind_release(axis, turn);
    }
    bind_event(input_->hold_mouse_steering, Triggered, [this] { update_mouse_steering(); });
    bind_release(input_->hold_mouse_steering, [this] { release_mouse_steering(); });
    bind_value(
        input_->accelerate, Triggered, ship_value(&sim::player::CommandInterface::set_accelerator));
    clear_axis(input_->accelerate, &sim::player::CommandInterface::set_accelerator);
    bind_event(input_->boost, Started, ship_event(&sim::player::CommandInterface::start_boost));
    bind_release(input_->boost, ship_event(&sim::player::CommandInterface::stop_boost));
    bind_event(input_->brake, Started, ship_event(&sim::player::CommandInterface::start_brake));
    bind_release(input_->brake, ship_event(&sim::player::CommandInterface::stop_brake));
    bind_event(input_->emergency_brake,
               Started,
               ship_event(&sim::player::CommandInterface::start_emergency_brake));
    bind_release(input_->emergency_brake,
                 ship_event(&sim::player::CommandInterface::stop_emergency_brake));
    bind_event(input_->fire_primary, Started, [this] { start_fire_primary(); });
    bind_release(input_->fire_primary, ship_event(&sim::player::CommandInterface::stop_fire_laser));
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
    if (mouse_displacement_) {
        mouse_displacement_.Reset();
        owner_->hud_.set_mouse_steering(mouse_displacement_);
    }
    auto* const ship{ship_.Get()};
    if (!IsValid(ship) || !ship->has_simulation()) {
        return;
    }
    ship->commands().set_pitch_input(0.f);
    ship->commands().set_yaw_input(0.f);
    ship->commands().set_accelerator(0.f);
    ship->commands().stop_boost();
    ship->commands().stop_brake();
    ship->commands().stop_emergency_brake();
    ship->commands().stop_fire_laser();
}
auto FShipControlContext::get_ship() const -> ATestSpaceShip* {
    auto* const ship{ship_.Get()};
    if (!IsValid(ship) || !ship->has_simulation()) {
        UE_LOG(LogSandboxController, Warning, TEXT("Player ship simulation is unavailable."));
        return nullptr;
    }
    return ship;
}
void FShipControlContext::update_gamepad_turn() {
    if (mouse_displacement_) {
        return;
    }
    auto const* const input{CastChecked<UEnhancedPlayerInput>(owner_->PlayerInput)};
    if (auto* const ship{get_ship()}) {
        auto const pitch{input->GetActionValue(input_->pitch).Get<float>()};
        if (spacegame::input_trace::is_enabled()) {
            UE_LOG(LogSandboxController,
                   Warning,
                   TEXT("[InputTrace] IA_Ship_Pitch -> native pitch intent %.3f"),
                   pitch);
        }
        ship->commands().set_pitch_input(pitch);
        ship->commands().set_yaw_input(input->GetActionValue(input_->yaw).Get<float>());
    }
}
void FShipControlContext::update_mouse_steering() {
    auto const* const input{CastChecked<UEnhancedPlayerInput>(owner_->PlayerInput)};
    FVector2D const delta{input->GetActionValue(input_->mouse_x).Get<float>(),
                          input->GetActionValue(input_->mouse_y).Get<float>()};
    if (mouse_displacement_ && delta.IsZero()) {
        return;
    }
    mouse_displacement_ = mouse_displacement_.Get(FVector2D::ZeroVector) + delta;
    owner_->hud_.set_mouse_steering(mouse_displacement_);

    auto const command{mouse_displacement_->GetClampedToMaxSize(1.0)};
    auto const* const settings{owner_->GetGameInstance()->GetSubsystem<UGameSettingsSubsystem>()};
    check(settings);
    auto const& preferences{settings->settings_state()};
    if (auto* const ship{get_ship()}) {
        ship->commands().set_pitch_input(preferences.invert_mouse_pitch ? -command.Y : command.Y);
        ship->commands().set_yaw_input(preferences.invert_mouse_yaw ? -command.X : command.X);
    }
}
void FShipControlContext::release_mouse_steering() {
    if (mouse_displacement_) {
        mouse_displacement_.Reset();
        owner_->hud_.set_mouse_steering(mouse_displacement_);
        update_gamepad_turn();
    }
}
void FShipControlContext::start_fire_primary() {
    if (auto* const ship{get_ship()}) {
        if (spacegame::input_trace::is_enabled()) {
            UE_LOG(LogSandboxController,
                   Warning,
                   TEXT("[InputTrace] IA_Ship_FirePrimary -> start_fire_laser"));
        }
        ship->commands().start_fire_laser();
    }
}

} // namespace ioj
