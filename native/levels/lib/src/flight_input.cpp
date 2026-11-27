#include <ioj/input/flight_input.h>

#include <ioj/input/response.h>
#include <ioj/vector_value_math.h>

#include <algorithm>
#include <cmath>

namespace ioj::input {
auto binding_name_equal(std::string_view left, std::string_view right) -> bool {
    // Existing profiles use FName identifiers, whose spelling is case-insensitive.
    auto const fold = [](char value) {
        return value >= 'A' && value <= 'Z' ? value + ('a' - 'A') : int{value};
    };
    return std::ranges::equal(left, right, {}, fold, fold);
}

auto flight_controls() -> std::span<FlightControl const> {
    using enum FlightAction;
    static constexpr FlightControl controls[]{
        {"General.Pause.KeyboardMouse", Pause, "Escape", "Pause", 10, false},
        {"General.Pause.Gamepad", Pause, "Gamepad_Special_Right", "Pause", 10, true},
        {"Starfox.Pitch.KeyboardMouse", PointerPitch, "MouseY", "Pitch", 10, false},
        {"Starfox.Pitch.Gamepad", Pitch, "Gamepad_LeftY", "Pitch", 10, true},
        {"Starfox.Yaw.KeyboardMouse", PointerYaw, "MouseX", "Yaw", 20, false},
        {"Starfox.Yaw.Gamepad", Yaw, "Gamepad_LeftX", "Yaw", 20, true},
        {"Starfox.Steer.KeyboardMouse", Steer, "RightMouseButton", "Hold to steer", 30, false},
        {"Starfox.Accelerate.KeyboardMouse", Accelerate, "W", "Accelerate", 40, false},
        {"Starfox.Accelerate.Gamepad",
         Accelerate,
         "Gamepad_LeftTriggerAxis",
         "Accelerate",
         40,
         true},
        {"Starfox.Brake.KeyboardMouse", Brake, "S", "Brake", 50, false},
        {"Starfox.Brake.Gamepad", Brake, "Gamepad_LeftShoulder", "Brake", 50, true},
        {"Starfox.Boost.KeyboardMouse", Boost, "LeftShift", "Boost", 60, false},
        {"Starfox.Boost.Gamepad", Boost, "Gamepad_RightShoulder", "Boost", 60, true},
        {"Starfox.EmergencyBrake.KeyboardMouse", EmergencyBrake, "X", "Emergency Brake", 70, false},
        {"Starfox.EmergencyBrake.Gamepad",
         EmergencyBrake,
         "Gamepad_FaceButton_Left",
         "Emergency Brake",
         70,
         true},
        {"Starfox.FirePrimary.KeyboardMouse", Fire, "LeftMouseButton", "Fire Primary", 100, false},
        {"Starfox.FirePrimary.Gamepad",
         Fire,
         "Gamepad_RightTriggerAxis",
         "Fire Primary",
         100,
         true},
    };
    return controls;
}
auto bound_key(FlightControl const& control, ControlsSettings const& settings) -> std::string_view {
    if (settings.bindings) {
        auto const found{std::ranges::find_if(*settings.bindings, [&](auto const& entry) {
            return binding_name_equal(entry.mapping, control.mapping);
        })};
        if (found != settings.bindings->end()) {
            return found->key;
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
        if (context == Context::Gameplay ||
            (context == Context::BenchmarkViewer && control.action == FlightAction::Pause)) {
            auto const key{bound_key(control, controls)};
            if (!key.empty()) {
                bindings_.push_back({control.action, std::string{key}, 0.f});
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
void FlightInput::restore_axis(std::string_view key, float position, ControlsSettings const& controls) {
    for (auto& binding : bindings_) {
        if ((binding.action == FlightAction::Pitch || binding.action == FlightAction::Yaw ||
             binding.action == FlightAction::Accelerate) &&
            binding_name_equal(binding.key, key)) {
            binding.value = position;
        }
    }
    update_axes(controls);
}
auto FlightInput::update(std::string_view key, float next, ControlsSettings const& controls)
    -> bool {
    using enum FlightAction;
    bool pause{};
    for (auto& binding : bindings_) {
        if (!binding_name_equal(binding.key, key)) {
            continue;
        }

        if ((binding.action == PointerPitch || binding.action == PointerYaw) &&
            (key == "MouseX" || key == "MouseY")) {
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
