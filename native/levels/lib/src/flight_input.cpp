#include <ioj/input/flight_input.h>

#include <ioj/input/response.h>
#include <ioj/vector_value_math.h>

#include <algorithm>
#include <cmath>
#include <utility>

namespace ioj::input {
auto accepts_binding(FlightMapping mapping, KeyCode key) -> bool {
    if (is_gamepad_mapping(mapping) != is_gamepad_control(key.key)) {
        return false;
    }
    switch (mapping) {
        case FlightMapping::GeneralPauseGamepad:
            return is_gamepad_button(key.key);
        case FlightMapping::GeneralPauseKeyboardMouse:
            return is_keyboard_mouse_button(key.key);
        case FlightMapping::StarfoxPitchKeyboardMouse:
        case FlightMapping::StarfoxYawKeyboardMouse:
            return is_keyboard_mouse_button(key.key) || key == PhysicalKey::MouseX ||
                   key == PhysicalKey::MouseY;
        case FlightMapping::StarfoxSteerKeyboardMouse:
        case FlightMapping::StarfoxAccelerateKeyboardMouse:
            return is_keyboard_mouse_button(key.key);
        default:
            return is_gamepad_control(key.key) || is_keyboard_mouse_button(key.key) ||
                   key == PhysicalKey::MouseScrollUp || key == PhysicalKey::MouseScrollDown;
    }
}
auto flight_controls() -> std::span<FlightControl const> {
    static constexpr FlightControl controls[]{
        {"Pause",
         PhysicalKey::Escape,
         FlightMapping::GeneralPauseKeyboardMouse,
         BindingGroup::Utility,
         10},
        {"Pause",
         PhysicalKey::GamepadSpecialRight,
         FlightMapping::GeneralPauseGamepad,
         BindingGroup::Utility,
         10},
        {"Pitch",
         PhysicalKey::MouseY,
         FlightMapping::StarfoxPitchKeyboardMouse,
         BindingGroup::Flight,
         10},
        {"Pitch",
         PhysicalKey::GamepadLeftY,
         FlightMapping::StarfoxPitchGamepad,
         BindingGroup::Flight,
         10},
        {"Yaw",
         PhysicalKey::MouseX,
         FlightMapping::StarfoxYawKeyboardMouse,
         BindingGroup::Flight,
         20},
        {"Yaw",
         PhysicalKey::GamepadLeftX,
         FlightMapping::StarfoxYawGamepad,
         BindingGroup::Flight,
         20},
        {"Hold to steer",
         PhysicalKey::RightMouseButton,
         FlightMapping::StarfoxSteerKeyboardMouse,
         BindingGroup::Flight,
         30},
        {"Accelerate",
         PhysicalKey::W,
         FlightMapping::StarfoxAccelerateKeyboardMouse,
         BindingGroup::Flight,
         40},
        {"Accelerate",
         PhysicalKey::GamepadLeftTriggerAxis,
         FlightMapping::StarfoxAccelerateGamepad,
         BindingGroup::Flight,
         40},
        {"Brake",
         PhysicalKey::S,
         FlightMapping::StarfoxBrakeKeyboardMouse,
         BindingGroup::Flight,
         50},
        {"Brake",
         PhysicalKey::GamepadLeftShoulder,
         FlightMapping::StarfoxBrakeGamepad,
         BindingGroup::Flight,
         50},
        {"Boost",
         PhysicalKey::LeftShift,
         FlightMapping::StarfoxBoostKeyboardMouse,
         BindingGroup::Flight,
         60},
        {"Boost",
         PhysicalKey::GamepadRightShoulder,
         FlightMapping::StarfoxBoostGamepad,
         BindingGroup::Flight,
         60},
        {"Emergency Brake",
         PhysicalKey::X,
         FlightMapping::StarfoxEmergencyBrakeKeyboardMouse,
         BindingGroup::Flight,
         70},
        {"Emergency Brake",
         PhysicalKey::GamepadFaceButtonLeft,
         FlightMapping::StarfoxEmergencyBrakeGamepad,
         BindingGroup::Flight,
         70},
        {"Fire Primary",
         PhysicalKey::LeftMouseButton,
         FlightMapping::StarfoxFirePrimaryKeyboardMouse,
         BindingGroup::Combat,
         100},
        {"Fire Primary",
         PhysicalKey::GamepadRightTriggerAxis,
         FlightMapping::StarfoxFirePrimaryGamepad,
         BindingGroup::Combat,
         100},
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
constexpr DigitalBinding digital_bindings[]{
    {FlightMapping::GeneralPauseKeyboardMouse, DigitalAction::Pause},
    {FlightMapping::GeneralPauseGamepad, DigitalAction::Pause},
    {FlightMapping::StarfoxSteerKeyboardMouse, DigitalAction::HoldToSteer},
    {FlightMapping::StarfoxBrakeKeyboardMouse, DigitalAction::Brake},
    {FlightMapping::StarfoxBrakeGamepad, DigitalAction::Brake},
    {FlightMapping::StarfoxBoostKeyboardMouse, DigitalAction::Boost},
    {FlightMapping::StarfoxBoostGamepad, DigitalAction::Boost},
    {FlightMapping::StarfoxEmergencyBrakeKeyboardMouse, DigitalAction::EmergencyBrake},
    {FlightMapping::StarfoxEmergencyBrakeGamepad, DigitalAction::EmergencyBrake},
    {FlightMapping::StarfoxFirePrimaryKeyboardMouse, DigitalAction::Fire},
    {FlightMapping::StarfoxFirePrimaryGamepad, DigitalAction::Fire},
};
constexpr std::array accelerator_mappings{FlightMapping::StarfoxAccelerateKeyboardMouse,
                                          FlightMapping::StarfoxAccelerateGamepad};
struct TurnBinding {
    FlightMapping mapping;
    Axis component;
};
constexpr TurnBinding pointer_bindings[]{
    {FlightMapping::StarfoxPitchKeyboardMouse, Axis::Pitch},
    {FlightMapping::StarfoxYawKeyboardMouse, Axis::Yaw},
};
constexpr TurnBinding stick_bindings[]{
    {FlightMapping::StarfoxPitchGamepad, Axis::Pitch},
    {FlightMapping::StarfoxYawGamepad, Axis::Yaw},
};
}

FlightBindings::FlightBindings(ControlsSettings const& controls, Context context) {
    for (auto const& control : flight_controls()) {
        if (context == Context::Gameplay || control.group == BindingGroup::Utility) {
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
                                    Context context) {
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
        active_digital_sources_.set(std::to_underlying(mapping), down);
        auto const active{
            down ||
            std::ranges::any_of(flight_binding_details::digital_bindings, [&](auto binding) {
                return binding.action == action &&
                       active_digital_sources_[std::to_underlying(binding.mapping)];
            })};
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
    if (edge == ButtonState::Repeated || key == PhysicalKey::Unbound) {
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
                    auto& component{binding.component == Axis::Yaw ? state.pointer_steering->x()
                                                                   : state.pointer_steering->y()};
                    component +=
                        apply_response(motion, Response::TurnPointerDelta, Axis::None, controls);
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
            (binding.component == Axis::Yaw ? gamepad_turn_.x() : gamepad_turn_.y()) = position;
        }
    }
    if (key != PhysicalKey::MouseX && key != PhysicalKey::MouseY) {
        for (auto binding : flight_binding_details::pointer_bindings) {
            if (bindings_.matches(binding.mapping, key)) {
                (binding.component == Axis::Yaw ? pointer_turn_.x() : pointer_turn_.y()) = position;
            }
        }
    }
    update_turn(state, controls);
}

void FlightInputResolver::update_turn(InputState& state, ControlsSettings const& controls) const {
    auto& turn{state.axes_2d[Axis2DAction::FlightTurn]};
    if (state.pointer_steering) {
        auto direction{*state.pointer_steering};
        direction.x() += pointer_turn_.x();
        direction.y() += pointer_turn_.y();
        auto const scale{std::max(1.0, size(direction))};
        turn.x() =
            static_cast<float>(direction.x() / scale) * (controls.invert_mouse_yaw ? -1.f : 1.f);
        turn.y() =
            static_cast<float>(direction.y() / scale) * (controls.invert_mouse_pitch ? -1.f : 1.f);
    } else {
        turn.x() = apply_response(gamepad_turn_.x(), Response::GamepadTurn, Axis::Yaw, controls);
        turn.y() = apply_response(gamepad_turn_.y(), Response::GamepadTurn, Axis::Pitch, controls);
    }
}

void FlightInputResolver::reset(InputState& state) {
    for (auto const binding : flight_binding_details::digital_bindings) {
        state.button(binding.action, ButtonState::Released);
    }
    active_digital_sources_.reset();
    gamepad_turn_ = {};
    pointer_turn_ = {};
    accelerator_sources_ = {};
    state.pointer_steering.reset();
    state.axes_1d[Axis1DAction::Accelerate] = {};
    state.axes_2d[Axis2DAction::FlightTurn] = {};
}
}
