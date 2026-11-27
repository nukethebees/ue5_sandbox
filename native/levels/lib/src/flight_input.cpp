#include <ioj/input/logical/flight_input.h>

#include <ioj/vector_value_math.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

namespace ioj {
auto accepts_binding(FlightMapping mapping, PhysicalKey key) -> bool {
    if (is_gamepad_mapping(mapping) != is_gamepad_control(key)) {
        return false;
    }
    switch (mapping) {
        case FlightMapping::GeneralPauseGamepad:
            return is_gamepad_button(key);
        case FlightMapping::GeneralPauseKeyboardMouse:
            return is_keyboard_mouse_button(key);
        case FlightMapping::FlightPitchKeyboardMouse:
        case FlightMapping::FlightYawKeyboardMouse:
            return is_keyboard_mouse_button(key) || key == PhysicalKey::MouseX ||
                   key == PhysicalKey::MouseY;
        case FlightMapping::FlightSteerKeyboardMouse:
        case FlightMapping::FlightAccelerateKeyboardMouse:
            return is_keyboard_mouse_button(key);
        default:
            return is_gamepad_control(key) || is_keyboard_mouse_button(key) ||
                   key == PhysicalKey::MouseScrollUp || key == PhysicalKey::MouseScrollDown;
    }
}
auto flight_controls() -> std::span<FlightControlDescriptor const> {
    static constexpr std::array controls{
        FlightControlDescriptor{
            "Pitch",
            PhysicalKey::MouseY,
            FlightMapping::FlightPitchKeyboardMouse,
            BindingGroup::Flight,
        },
        FlightControlDescriptor{
            "Pitch",
            PhysicalKey::GamepadLeftY,
            FlightMapping::FlightPitchGamepad,
            BindingGroup::Flight,
        },
        FlightControlDescriptor{
            "Yaw",
            PhysicalKey::MouseX,
            FlightMapping::FlightYawKeyboardMouse,
            BindingGroup::Flight,
        },
        FlightControlDescriptor{
            "Yaw",
            PhysicalKey::GamepadLeftX,
            FlightMapping::FlightYawGamepad,
            BindingGroup::Flight,
        },
        FlightControlDescriptor{
            "Hold to steer",
            PhysicalKey::RightMouseButton,
            FlightMapping::FlightSteerKeyboardMouse,
            BindingGroup::Flight,
        },
        FlightControlDescriptor{
            "Accelerate",
            PhysicalKey::W,
            FlightMapping::FlightAccelerateKeyboardMouse,
            BindingGroup::Flight,
        },
        FlightControlDescriptor{
            "Accelerate",
            PhysicalKey::GamepadLeftTriggerAxis,
            FlightMapping::FlightAccelerateGamepad,
            BindingGroup::Flight,
        },
        FlightControlDescriptor{
            "Brake",
            PhysicalKey::S,
            FlightMapping::FlightBrakeKeyboardMouse,
            BindingGroup::Flight,
        },
        FlightControlDescriptor{
            "Brake",
            PhysicalKey::GamepadLeftShoulder,
            FlightMapping::FlightBrakeGamepad,
            BindingGroup::Flight,
        },
        FlightControlDescriptor{
            "Boost",
            PhysicalKey::LeftShift,
            FlightMapping::FlightBoostKeyboardMouse,
            BindingGroup::Flight,
        },
        FlightControlDescriptor{
            "Boost",
            PhysicalKey::GamepadRightShoulder,
            FlightMapping::FlightBoostGamepad,
            BindingGroup::Flight,
        },
        FlightControlDescriptor{
            "Emergency Brake",
            PhysicalKey::X,
            FlightMapping::FlightEmergencyBrakeKeyboardMouse,
            BindingGroup::Flight,
        },
        FlightControlDescriptor{
            "Emergency Brake",
            PhysicalKey::GamepadFaceButtonLeft,
            FlightMapping::FlightEmergencyBrakeGamepad,
            BindingGroup::Flight,
        },
        FlightControlDescriptor{
            "Fire Primary",
            PhysicalKey::LeftMouseButton,
            FlightMapping::FlightFirePrimaryKeyboardMouse,
            BindingGroup::Combat,
        },
        FlightControlDescriptor{
            "Fire Primary",
            PhysicalKey::GamepadRightTriggerAxis,
            FlightMapping::FlightFirePrimaryGamepad,
            BindingGroup::Combat,
        },
        FlightControlDescriptor{
            "Pause",
            PhysicalKey::Escape,
            FlightMapping::GeneralPauseKeyboardMouse,
            BindingGroup::Utility,
        },
        FlightControlDescriptor{
            "Pause",
            PhysicalKey::GamepadSpecialRight,
            FlightMapping::GeneralPauseGamepad,
            BindingGroup::Utility,
        },
    };
    return controls;
}
auto default_flight_bindings() -> FlightBindings {
    FlightBindings bindings;
    for (auto const& control : flight_controls()) {
        bindings[control.mapping] = control.default_key;
    }
    return bindings;
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

auto is_pause_button(FlightBindings const& bindings, PhysicalKey key) -> bool {
    return key != PhysicalKey::Unbound &&
           (bindings[FlightMapping::GeneralPauseGamepad] == key ||
            bindings[FlightMapping::GeneralPauseKeyboardMouse] == key);
}

namespace {
void update_turn(FlightInputState const& flight,
                 LogicalInputState& state,
                 ControlsSettings const& controls) {
    auto& turn{state.axes_2d[Axis2DAction::FlightTurn]};
    if (state.pointer_steering) {
        auto direction{*state.pointer_steering};
        direction.x() += flight.keyboard_turn.x();
        direction.y() += flight.keyboard_turn.y();
        auto const scale{std::max(1.0, size(direction))};
        turn.x() =
            static_cast<float>(direction.x() / scale) * (controls.invert_mouse_yaw ? -1.f : 1.f);
        turn.y() =
            static_cast<float>(direction.y() / scale) * (controls.invert_mouse_pitch ? -1.f : 1.f);
    } else {
        turn.x() = apply_response(
            flight.gamepad_turn.x(), InputResponse::GamepadTurn, FlightAxis::Yaw, controls);
        turn.y() = apply_response(
            flight.gamepad_turn.y(), InputResponse::GamepadTurn, FlightAxis::Pitch, controls);
    }
}

void resolve_digital(FlightInputState& flight,
                     LogicalInputState& state,
                     PhysicalKey key,
                     bool down,
                     ControlsSettings const& controls,
                     InputContext context) {
    for (auto const [mapping, action] : flight_binding_details::digital_bindings) {
        if ((context != InputContext::Gameplay && action != DigitalAction::Pause) ||
            controls.bindings[mapping] != key) {
            continue;
        }
        auto& held{is_gamepad_mapping(mapping) ? flight.gamepad_held : flight.keyboard_mouse_held};
        held.set(action, down);
        auto const active{flight.keyboard_mouse_held.get(action) ||
                          flight.gamepad_held.get(action)};
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

}
void map_flight_button(FlightInputState& flight,
                       LogicalInputState& state,
                       PhysicalKey key,
                       ButtonState edge,
                       ControlsSettings const& controls,
                       InputContext context) {
    if (key == PhysicalKey::Unbound) {
        return;
    }
    auto const down{edge == ButtonState::Pressed};
    resolve_digital(flight, state, key, down, controls, context);
    if (context == InputContext::Gameplay) {
        map_flight_axis(flight, state, key, down ? 1.f : 0.f, controls);
    }
}

void map_flight_pointer(FlightInputState& flight,
                        LogicalInputState& state,
                        Vector2f delta,
                        ControlsSettings const& controls) {
    for (auto key : {PhysicalKey::MouseX, PhysicalKey::MouseY}) {
        auto const motion{key == PhysicalKey::MouseX ? delta.x() : delta.y()};
        if (state.pointer_steering) {
            for (auto const binding : flight_binding_details::pointer_bindings) {
                if (controls.bindings[binding.mapping] == key) {
                    auto& component{binding.component == FlightAxis::Yaw
                                        ? state.pointer_steering->x()
                                        : state.pointer_steering->y()};
                    component += apply_response(
                        motion, InputResponse::TurnPointerDelta, FlightAxis::None, controls);
                }
            }
        }
    }
    update_turn(flight, state, controls);
}

void map_flight_axis(FlightInputState& flight,
                     LogicalInputState& state,
                     PhysicalKey key,
                     float position,
                     ControlsSettings const& controls) {
    if (controls.bindings[FlightMapping::FlightAccelerateKeyboardMouse] == key) {
        flight.keyboard_mouse_acceleration = position;
    }
    if (controls.bindings[FlightMapping::FlightAccelerateGamepad] == key) {
        flight.gamepad_acceleration = position;
    }
    auto const strongest{std::abs(flight.gamepad_acceleration) >
                                 std::abs(flight.keyboard_mouse_acceleration)
                             ? flight.gamepad_acceleration
                             : flight.keyboard_mouse_acceleration};
    state.axes_1d[Axis1DAction::Accelerate] = std::clamp(strongest, 0.f, 1.f);
    for (auto binding : flight_binding_details::stick_bindings) {
        if (controls.bindings[binding.mapping] == key) {
            (binding.component == FlightAxis::Yaw ? flight.gamepad_turn.x()
                                                  : flight.gamepad_turn.y()) = position;
        }
    }
    if (key != PhysicalKey::MouseX && key != PhysicalKey::MouseY) {
        for (auto binding : flight_binding_details::pointer_bindings) {
            if (controls.bindings[binding.mapping] == key) {
                (binding.component == FlightAxis::Yaw ? flight.keyboard_turn.x()
                                                      : flight.keyboard_turn.y()) = position;
            }
        }
    }
    update_turn(flight, state, controls);
}

void reset_flight_input(FlightInputState& flight, LogicalInputState& state) {
    for (auto const binding : flight_binding_details::digital_bindings) {
        state.button(binding.action, ButtonState::Released);
    }
    flight = {};
    state.pointer_steering.reset();
    state.axes_1d[Axis1DAction::Accelerate] = {};
    state.axes_2d[Axis2DAction::FlightTurn] = {};
}
}
