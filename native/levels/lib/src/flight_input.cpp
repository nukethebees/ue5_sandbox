#include <ioj/input/flight_input.h>

#include <ioj/input/response.h>
#include <ioj/vector_value_math.h>

#include <algorithm>
#include <cmath>

namespace ioj::input {
auto accepts_binding(FlightMapping mapping, KeyCode key) -> bool {
    if (key == PhysicalKey::Unbound) {
        return false;
    }
    switch (mapping) {
        case FlightMapping::GeneralPauseGamepad:
            return is_gamepad_button(key.key);
        case FlightMapping::GeneralPauseKeyboardMouse:
            return is_keyboard_mouse_button(key.key);
        default:
            return is_gamepad_mapping(mapping) == is_gamepad_control(key.key);
    }
}
auto flight_controls() -> std::span<FlightControl const> {
    using enum FlightAction;
    static constexpr FlightControl controls[]{
        {FlightMapping::GeneralPauseKeyboardMouse, Pause, PhysicalKey::Escape, "Pause", 10},
        {FlightMapping::GeneralPauseGamepad, Pause, PhysicalKey::GamepadSpecialRight, "Pause", 10},
        {FlightMapping::StarfoxPitchKeyboardMouse, PointerPitch, PhysicalKey::MouseY, "Pitch", 10},
        {FlightMapping::StarfoxPitchGamepad, Pitch, PhysicalKey::GamepadLeftY, "Pitch", 10},
        {FlightMapping::StarfoxYawKeyboardMouse, PointerYaw, PhysicalKey::MouseX, "Yaw", 20},
        {FlightMapping::StarfoxYawGamepad, Yaw, PhysicalKey::GamepadLeftX, "Yaw", 20},
        {FlightMapping::StarfoxSteerKeyboardMouse,
         Steer,
         PhysicalKey::RightMouseButton,
         "Hold to steer",
         30},
        {FlightMapping::StarfoxAccelerateKeyboardMouse,
         Accelerate,
         PhysicalKey::W,
         "Accelerate",
         40},
        {FlightMapping::StarfoxAccelerateGamepad,
         Accelerate,
         PhysicalKey::GamepadLeftTriggerAxis,
         "Accelerate",
         40},
        {FlightMapping::StarfoxBrakeKeyboardMouse, Brake, PhysicalKey::S, "Brake", 50},
        {FlightMapping::StarfoxBrakeGamepad, Brake, PhysicalKey::GamepadLeftShoulder, "Brake", 50},
        {FlightMapping::StarfoxBoostKeyboardMouse, Boost, PhysicalKey::LeftShift, "Boost", 60},
        {FlightMapping::StarfoxBoostGamepad, Boost, PhysicalKey::GamepadRightShoulder, "Boost", 60},
        {FlightMapping::StarfoxEmergencyBrakeKeyboardMouse,
         EmergencyBrake,
         PhysicalKey::X,
         "Emergency Brake",
         70},
        {FlightMapping::StarfoxEmergencyBrakeGamepad,
         EmergencyBrake,
         PhysicalKey::GamepadFaceButtonLeft,
         "Emergency Brake",
         70},
        {FlightMapping::StarfoxFirePrimaryKeyboardMouse,
         Fire,
         PhysicalKey::LeftMouseButton,
         "Fire Primary",
         100},
        {FlightMapping::StarfoxFirePrimaryGamepad,
         Fire,
         PhysicalKey::GamepadRightTriggerAxis,
         "Fire Primary",
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
FlightInput::FlightInput()
    : events_{*std::pmr::get_default_resource()} {}
void FlightInput::configure(ControlsSettings const& controls, Context context) {
    reset();
    bindings_.clear();
    for (auto const& control : flight_controls()) {
        if (context == Context::Gameplay || control.action == FlightAction::Pause) {
            auto const key{bound_key(control, controls)};
            if (key != PhysicalKey::Unbound) {
                bindings_.push_back({control.action, key, 0.f});
            }
        }
    }
}
auto FlightInput::value(FlightAction action) const -> float {
    float strongest{};
    for (auto const& binding : bindings_) {
        if (binding.action == action && std::abs(binding.value) > std::abs(strongest)) {
            strongest = binding.value;
        }
    }
    return strongest;
}
auto FlightInput::is_pause_button(KeyCode key) const -> bool {
    return std::ranges::any_of(bindings_, [&](auto const& binding) {
        return binding.action == FlightAction::Pause && binding.key == key;
    });
}
void FlightInput::restore_axis(KeyCode key, float position, ControlsSettings const& controls) {
    for (auto& binding : bindings_) {
        if ((binding.action == FlightAction::Pitch || binding.action == FlightAction::Yaw ||
             binding.action == FlightAction::Accelerate) &&
            binding.key == key) {
            binding.value = position;
        }
    }
    update_axes(controls);
}
auto FlightInput::update(KeyCode key, float next, ControlsSettings const& controls) -> bool {
    using enum FlightAction;
    bool pause{};
    for (auto& binding : bindings_) {
        if (binding.key != key) {
            continue;
        }

        if ((binding.action == PointerPitch || binding.action == PointerYaw) &&
            (key == PhysicalKey::MouseX || key == PhysicalKey::MouseY)) {
            if (steering_) {
                auto const delta{
                    apply_response(next, Response::TurnPointerDelta, Axis::None, controls)};
                if (binding.action == PointerPitch) {
                    steering_->y() += delta;
                } else {
                    steering_->x() += delta;
                }
            }
            continue;
        }

        auto const was_down{std::abs(value(binding.action)) >= 0.5f};
        binding.value = next;
        auto const down{std::abs(value(binding.action)) >= 0.5f};
        if (was_down == down) {
            continue;
        }

        switch (binding.action) {
            case Boost:
                down ? events_.start_boost() : events_.stop_boost();
                break;
            case Brake:
                down ? events_.start_brake() : events_.stop_brake();
                break;
            case EmergencyBrake:
                down ? events_.start_emergency_brake() : events_.stop_emergency_brake();
                break;
            case Fire:
                down ? events_.start_fire_laser() : events_.stop_fire_laser();
                break;
            case Pause:
                pause = down;
                break;
            case Steer:
                if (down) {
                    steering_.emplace();
                } else {
                    steering_.reset();
                }
                break;
            default:
                break;
        }
    }
    update_axes(controls);
    return pause;
}
void FlightInput::update_axes(ControlsSettings const& controls) {
    if (steering_) {
        auto direction{*steering_};
        direction.x() += value(FlightAction::PointerYaw);
        direction.y() += value(FlightAction::PointerPitch);
        auto const scale{std::max(1.0, size(direction))};
        events_.set_pitch_input(static_cast<float>(direction.y() / scale) *
                                (controls.invert_mouse_pitch ? -1.f : 1.f));
        events_.set_yaw_input(static_cast<float>(direction.x() / scale) *
                              (controls.invert_mouse_yaw ? -1.f : 1.f));
    } else {
        events_.set_pitch_input(apply_response(
            value(FlightAction::Pitch), Response::GamepadTurn, Axis::Pitch, controls));
        events_.set_yaw_input(
            apply_response(value(FlightAction::Yaw), Response::GamepadTurn, Axis::Yaw, controls));
    }
    events_.set_accelerator(value(FlightAction::Accelerate));
}
void FlightInput::reset() {
    using enum FlightAction;
    if (std::abs(value(Fire)) >= 0.5f) {
        events_.stop_fire_laser();
    }
    if (std::abs(value(Boost)) >= 0.5f) {
        events_.stop_boost();
    }
    if (std::abs(value(Brake)) >= 0.5f) {
        events_.stop_brake();
    }
    if (std::abs(value(EmergencyBrake)) >= 0.5f) {
        events_.stop_emergency_brake();
    }
    for (auto& binding : bindings_) {
        binding.value = 0.f;
    }
    steering_.reset();
    events_.set_pitch_input(0.f);
    events_.set_yaw_input(0.f);
    events_.set_accelerator(0.f);
}
void FlightInput::begin_frame() {
    events_.clear_events();
}
}
