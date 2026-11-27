#include <ioj/input/flight_input.h>

#include <ioj/input/response.h>
#include <ioj/vector_value_math.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

namespace ioj {
auto accepts_binding(FlightMapping mapping, KeyCode key) -> bool {
    if (is_gamepad_mapping(mapping) != is_gamepad_control(key.key)) {
        return false;
    }
    switch (mapping) {
        case FlightMapping::GeneralPauseGamepad:
            return is_gamepad_button(key.key);
        case FlightMapping::GeneralPauseKeyboardMouse:
            return is_keyboard_mouse_button(key.key);
        case FlightMapping::FlightPitchKeyboardMouse:
        case FlightMapping::FlightYawKeyboardMouse:
            return is_keyboard_mouse_button(key.key) || key == PhysicalKey::MouseX ||
                   key == PhysicalKey::MouseY;
        case FlightMapping::FlightSteerKeyboardMouse:
        case FlightMapping::FlightAccelerateKeyboardMouse:
            return is_keyboard_mouse_button(key.key);
        default:
            return is_gamepad_control(key.key) || is_keyboard_mouse_button(key.key) ||
                   key == PhysicalKey::MouseScrollUp || key == PhysicalKey::MouseScrollDown;
    }
}
auto flight_controls() -> std::span<FlightControl const> {
    static constexpr std::array controls{
        FlightControl{
            "Pitch",
            PhysicalKey::MouseY,
            FlightMapping::FlightPitchKeyboardMouse,
            BindingGroup::Flight,
        },
        FlightControl{
            "Pitch",
            PhysicalKey::GamepadLeftY,
            FlightMapping::FlightPitchGamepad,
            BindingGroup::Flight,
        },
        FlightControl{
            "Yaw",
            PhysicalKey::MouseX,
            FlightMapping::FlightYawKeyboardMouse,
            BindingGroup::Flight,
        },
        FlightControl{
            "Yaw",
            PhysicalKey::GamepadLeftX,
            FlightMapping::FlightYawGamepad,
            BindingGroup::Flight,
        },
        FlightControl{
            "Hold to steer",
            PhysicalKey::RightMouseButton,
            FlightMapping::FlightSteerKeyboardMouse,
            BindingGroup::Flight,
        },
        FlightControl{
            "Accelerate",
            PhysicalKey::W,
            FlightMapping::FlightAccelerateKeyboardMouse,
            BindingGroup::Flight,
        },
        FlightControl{
            "Accelerate",
            PhysicalKey::GamepadLeftTriggerAxis,
            FlightMapping::FlightAccelerateGamepad,
            BindingGroup::Flight,
        },
        FlightControl{
            "Brake",
            PhysicalKey::S,
            FlightMapping::FlightBrakeKeyboardMouse,
            BindingGroup::Flight,
        },
        FlightControl{
            "Brake",
            PhysicalKey::GamepadLeftShoulder,
            FlightMapping::FlightBrakeGamepad,
            BindingGroup::Flight,
        },
        FlightControl{
            "Boost",
            PhysicalKey::LeftShift,
            FlightMapping::FlightBoostKeyboardMouse,
            BindingGroup::Flight,
        },
        FlightControl{
            "Boost",
            PhysicalKey::GamepadRightShoulder,
            FlightMapping::FlightBoostGamepad,
            BindingGroup::Flight,
        },
        FlightControl{
            "Emergency Brake",
            PhysicalKey::X,
            FlightMapping::FlightEmergencyBrakeKeyboardMouse,
            BindingGroup::Flight,
        },
        FlightControl{
            "Emergency Brake",
            PhysicalKey::GamepadFaceButtonLeft,
            FlightMapping::FlightEmergencyBrakeGamepad,
            BindingGroup::Flight,
        },
        FlightControl{
            "Fire Primary",
            PhysicalKey::LeftMouseButton,
            FlightMapping::FlightFirePrimaryKeyboardMouse,
            BindingGroup::Combat,
        },
        FlightControl{
            "Fire Primary",
            PhysicalKey::GamepadRightTriggerAxis,
            FlightMapping::FlightFirePrimaryGamepad,
            BindingGroup::Combat,
        },
        FlightControl{
            "Pause",
            PhysicalKey::Escape,
            FlightMapping::GeneralPauseKeyboardMouse,
            BindingGroup::Utility,
        },
        FlightControl{
            "Pause",
            PhysicalKey::GamepadSpecialRight,
            FlightMapping::GeneralPauseGamepad,
            BindingGroup::Utility,
        },
    };
    return controls;
}
auto bound_key(FlightControl const& control, ControlsSettings const& settings) -> KeyCode {
    if (settings.bindings) {
        auto const found{
            std::ranges::find(*settings.bindings, control.mapping, &KeyBinding::mapping)};
        if (found != settings.bindings->end()) {
            if (found->key == PhysicalKey::Unbound) {
                return {};
            }
            auto const key{found->key};
            return accepts_binding(control.mapping, key) ? key : control.default_key;
        }
    }
    return control.default_key;
}

namespace flight_binding_details {
struct DigitalBinding {
    FlightMapping mapping;
    DigitalAction action;
};
constexpr std::array digital_bindings{
    DigitalBinding{FlightMapping::GeneralPauseKeyboardMouse, DigitalAction::Pause},
    DigitalBinding{FlightMapping::GeneralPauseGamepad, DigitalAction::Pause},
    DigitalBinding{FlightMapping::FlightSteerKeyboardMouse, DigitalAction::HoldToSteer},
    DigitalBinding{FlightMapping::FlightBrakeKeyboardMouse, DigitalAction::Brake},
    DigitalBinding{FlightMapping::FlightBrakeGamepad, DigitalAction::Brake},
    DigitalBinding{FlightMapping::FlightBoostKeyboardMouse, DigitalAction::Boost},
    DigitalBinding{FlightMapping::FlightBoostGamepad, DigitalAction::Boost},
    DigitalBinding{FlightMapping::FlightEmergencyBrakeKeyboardMouse, DigitalAction::EmergencyBrake},
    DigitalBinding{FlightMapping::FlightEmergencyBrakeGamepad, DigitalAction::EmergencyBrake},
    DigitalBinding{FlightMapping::FlightFirePrimaryKeyboardMouse, DigitalAction::Fire},
    DigitalBinding{FlightMapping::FlightFirePrimaryGamepad, DigitalAction::Fire},
};
constexpr std::array accelerator_mappings{FlightMapping::FlightAccelerateKeyboardMouse,
                                          FlightMapping::FlightAccelerateGamepad};
struct TurnBinding {
    FlightMapping mapping;
    FlightAxis component;
};
constexpr std::array pointer_bindings{
    TurnBinding{FlightMapping::FlightPitchKeyboardMouse, FlightAxis::Pitch},
    TurnBinding{FlightMapping::FlightYawKeyboardMouse, FlightAxis::Yaw},
};
constexpr std::array stick_bindings{
    TurnBinding{FlightMapping::FlightPitchGamepad, FlightAxis::Pitch},
    TurnBinding{FlightMapping::FlightYawGamepad, FlightAxis::Yaw},
};
}

FlightBindings::FlightBindings(ControlsSettings const& controls, InputContext context) {
    for (auto const& control : flight_controls()) {
        if (context == InputContext::Gameplay || control.group == BindingGroup::Utility) {
            auto const key{bound_key(control, controls)};
            bound_keys_[control.mapping] = key.key;
            bound_characters_[control.mapping] = key.codepoint;
        }
    }
}
auto FlightBindings::matches(FlightMapping mapping, KeyCode key) const -> bool {
    return bound_keys_[mapping] == key.key && bound_characters_[mapping] == key.codepoint;
}

void FlightInputResolver::configure(InputState& state,
                                    ControlsSettings const& controls,
                                    InputContext context) {
    reset(state);
    bindings_ = FlightBindings{controls, context};
}
auto FlightInputResolver::is_pause_button(KeyCode key) const -> bool {
    return key != PhysicalKey::Unbound &&
           (bindings_.matches(FlightMapping::GeneralPauseGamepad, key) ||
            bindings_.matches(FlightMapping::GeneralPauseKeyboardMouse, key));
}

void FlightInputResolver::resolve_digital(InputState& state, KeyCode key, bool down) {
    for (auto const [mapping, action] : flight_binding_details::digital_bindings) {
        if (!bindings_.matches(mapping, key)) {
            continue;
        }
        auto& held{is_gamepad_mapping(mapping) ? gamepad_held_ : keyboard_mouse_held_};
        held.set(action, down);
        auto const active{keyboard_mouse_held_.get(action) || gamepad_held_.get(action)};
        if (action == DigitalAction::HoldToSteer && active != state.pointer_steering.has_value()) {
            if (active) {
                state.pointer_steering.emplace();
            } else {
                state.pointer_steering.reset();
            }
        }
        state.button(action, active ? ButtonState::Pressed : ButtonState::Released);
    }
}

void FlightInputResolver::button(InputState& state,
                                 KeyCode key,
                                 ButtonState edge,
                                 ControlsSettings const& controls) {
    if (key == PhysicalKey::Unbound) {
        return;
    }
    auto const down{edge == ButtonState::Pressed};
    resolve_digital(state, key, down);
    sample_axis(state, key, down ? 1.f : 0.f, controls);
}

void FlightInputResolver::pointer_motion(InputState& state,
                                         Vector2f delta,
                                         ControlsSettings const& controls) {
    for (auto key : {PhysicalKey::MouseX, PhysicalKey::MouseY}) {
        auto const motion{key == PhysicalKey::MouseX ? delta.x() : delta.y()};
        if (state.pointer_steering) {
            for (auto const binding : flight_binding_details::pointer_bindings) {
                if (bindings_.matches(binding.mapping, key)) {
                    auto& component{binding.component == FlightAxis::Yaw ? state.pointer_steering->x()
                                                                   : state.pointer_steering->y()};
                    component +=
                        apply_response(motion, InputResponse::TurnPointerDelta, FlightAxis::None, controls);
                }
            }
        }
    }
    update_turn(state, controls);
}

void FlightInputResolver::sample_axis(InputState& state,
                                      KeyCode key,
                                      float position,
                                      ControlsSettings const& controls) {
    for (std::size_t i{}; i < flight_binding_details::accelerator_mappings.size(); ++i) {
        if (bindings_.matches(flight_binding_details::accelerator_mappings[i], key)) {
            accelerator_sources_[i] = position;
        }
    }
    auto const strongest{std::abs(accelerator_sources_[1]) > std::abs(accelerator_sources_[0])
                             ? accelerator_sources_[1]
                             : accelerator_sources_[0]};
    state.axes_1d[Axis1DAction::Accelerate] = std::clamp(strongest, 0.f, 1.f);
    for (auto binding : flight_binding_details::stick_bindings) {
        if (bindings_.matches(binding.mapping, key)) {
            (binding.component == FlightAxis::Yaw ? gamepad_turn_.x() : gamepad_turn_.y()) = position;
        }
    }
    if (key != PhysicalKey::MouseX && key != PhysicalKey::MouseY) {
        for (auto binding : flight_binding_details::pointer_bindings) {
            if (bindings_.matches(binding.mapping, key)) {
                (binding.component == FlightAxis::Yaw ? keyboard_turn_.x() : keyboard_turn_.y()) = position;
            }
        }
    }
    update_turn(state, controls);
}

void FlightInputResolver::update_turn(InputState& state, ControlsSettings const& controls) const {
    auto& turn{state.axes_2d[Axis2DAction::FlightTurn]};
    if (state.pointer_steering) {
        auto direction{*state.pointer_steering};
        direction.x() += keyboard_turn_.x();
        direction.y() += keyboard_turn_.y();
        auto const scale{std::max(1.0, size(direction))};
        turn.x() =
            static_cast<float>(direction.x() / scale) * (controls.invert_mouse_yaw ? -1.f : 1.f);
        turn.y() =
            static_cast<float>(direction.y() / scale) * (controls.invert_mouse_pitch ? -1.f : 1.f);
    } else {
        turn.x() = apply_response(gamepad_turn_.x(), InputResponse::GamepadTurn, FlightAxis::Yaw, controls);
        turn.y() = apply_response(gamepad_turn_.y(), InputResponse::GamepadTurn, FlightAxis::Pitch, controls);
    }
}

void FlightInputResolver::reset(InputState& state) {
    for (auto const binding : flight_binding_details::digital_bindings) {
        state.button(binding.action, ButtonState::Released);
    }
    keyboard_mouse_held_ = {};
    gamepad_held_ = {};
    gamepad_turn_ = {};
    keyboard_turn_ = {};
    accelerator_sources_ = {};
    state.pointer_steering.reset();
    state.axes_1d[Axis1DAction::Accelerate] = {};
    state.axes_2d[Axis2DAction::FlightTurn] = {};
}
}
