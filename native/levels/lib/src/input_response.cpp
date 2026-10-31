#include "ioj/input/response.h"
#include "ioj/vector_value_math.h"

#include <algorithm>
#include <cmath>

namespace ioj::input {
namespace {
auto inverted(Response const response, Axis const axis, GameSettingsState const& settings) noexcept
    -> bool {
    if (response == Response::TurnPointerDelta) {
        switch (axis) {
            case Axis::Pitch:
                return settings.invert_mouse_pitch;
            case Axis::Yaw:
                return settings.invert_mouse_yaw;
            default:
                return false;
        }
    }
    switch (axis) {
        case Axis::Pitch:
            return settings.invert_gamepad_pitch;
        case Axis::Yaw:
            return settings.invert_gamepad_yaw;
        case Axis::Roll:
            return settings.invert_gamepad_roll;
        case Axis::VerticalTranslation:
            return settings.invert_gamepad_vertical_translation;
        case Axis::None:
            return false;
    }
    return false;
}

auto scale(Response const response, GameSettingsState const& settings) noexcept -> double {
    switch (response) {
        case Response::TurnPointerDelta:
            return clamp_sensitivity(settings.mouse_turn_sensitivity) / 400.0;
        case Response::GamepadTurn:
            return clamp_sensitivity(settings.gamepad_turn_sensitivity);
        case Response::GamepadMove:
            return 1.0;
    }
    return 1.0;
}

auto dead_zone(Response const response, GameSettingsState const& settings) noexcept -> double {
    return clamp_dead_zone(response == Response::GamepadTurn ? settings.gamepad_turn_dead_zone
                                                             : settings.gamepad_move_dead_zone);
}

template <std::size_t N>
auto apply_vector_response(Vector<double, N> value,
                           Response const response,
                           Axis const axis,
                           GameSettingsState const& settings) noexcept -> Vector<double, N> {
    if (response != Response::TurnPointerDelta) {
        auto const threshold{dead_zone(response, settings)};
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
                    Response const response,
                    Axis const axis,
                    GameSettingsState const& settings) noexcept -> float {
    if (response != Response::TurnPointerDelta) {
        auto const threshold{dead_zone(response, settings)};
        auto const magnitude{std::abs(value)};
        value = magnitude <= threshold ? 0.0f
                                       : static_cast<float>(std::copysign(
                                             (magnitude - threshold) / (1.0 - threshold), value));
    }
    return static_cast<float>(value * scale(response, settings) *
                              (inverted(response, axis, settings) ? -1.0 : 1.0));
}
auto apply_response(Vector2d const value,
                    Response const response,
                    Axis const axis,
                    GameSettingsState const& settings) noexcept -> Vector2d {
    return apply_vector_response(value, response, axis, settings);
}
auto apply_response(Vector3d const value,
                    Response const response,
                    Axis const axis,
                    GameSettingsState const& settings) noexcept -> Vector3d {
    return apply_vector_response(value, response, axis, settings);
}
}
