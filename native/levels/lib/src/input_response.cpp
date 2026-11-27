#include "ioj/input/logical/response.h"
#include "ioj/vector_value_math.h"

#include <algorithm>
#include <cmath>

namespace ioj {
namespace {
auto inverted(InputResponse const response,
              FlightAxis const axis,
              ControlsSettings const& settings) noexcept -> bool {
    if (response == InputResponse::TurnPointerDelta) {
        switch (axis) {
            case FlightAxis::Pitch:
                return settings.invert_mouse_pitch;
            case FlightAxis::Yaw:
                return settings.invert_mouse_yaw;
            default:
                return false;
        }
    }
    switch (axis) {
        case FlightAxis::Pitch:
            return settings.invert_gamepad_pitch;
        case FlightAxis::Yaw:
            return settings.invert_gamepad_yaw;
        case FlightAxis::None:
            return false;
    }
    return false;
}

auto scale(InputResponse const response, ControlsSettings const& settings) noexcept -> double {
    switch (response) {
        case InputResponse::TurnPointerDelta:
            return clamp_sensitivity(settings.mouse_turn_sensitivity) / 400.0;
        case InputResponse::GamepadTurn:
            return clamp_sensitivity(settings.gamepad_turn_sensitivity);
    }
    return 1.0;
}

template <std::size_t N>
auto apply_vector_response(Vector<double, N> value,
                           InputResponse const response,
                           FlightAxis const axis,
                           ControlsSettings const& settings) noexcept -> Vector<double, N> {
    if (response != InputResponse::TurnPointerDelta) {
        auto const threshold{clamp_dead_zone(settings.gamepad_turn_dead_zone)};
        auto const magnitude{size(value)};
        if (magnitude <= threshold) {
            return {};
        }
        auto const output_magnitude{std::min((magnitude - threshold) / (1.0 - threshold), 1.0)};
        value *= output_magnitude / magnitude;
    }
    value *= scale(response, settings);
    if (inverted(response, axis, settings)) {
        value.y() = -value.y();
    }
    return value;
}
}

auto clamp_sensitivity(float const value) noexcept -> float {
    return std::max(0.0f, value);
}
auto clamp_dead_zone(float const value) noexcept -> float {
    return std::clamp(value, 0.0f, 0.95f);
}

auto apply_response(float value,
                    InputResponse const response,
                    FlightAxis const axis,
                    ControlsSettings const& settings) noexcept -> float {
    if (response != InputResponse::TurnPointerDelta) {
        auto const threshold{clamp_dead_zone(settings.gamepad_turn_dead_zone)};
        auto const magnitude{std::abs(value)};
        value = magnitude <= threshold ? 0.0f
                                       : static_cast<float>(std::copysign(
                                             (magnitude - threshold) / (1.0 - threshold), value));
    }
    return static_cast<float>(value * scale(response, settings) *
                              (inverted(response, axis, settings) ? -1.0 : 1.0));
}
auto apply_response(Vector2d const value,
                    InputResponse const response,
                    FlightAxis const axis,
                    ControlsSettings const& settings) noexcept -> Vector2d {
    return apply_vector_response(value, response, axis, settings);
}
auto apply_response(Vector3d const value,
                    InputResponse const response,
                    FlightAxis const axis,
                    ControlsSettings const& settings) noexcept -> Vector3d {
    return apply_vector_response(value, response, axis, settings);
}
}
