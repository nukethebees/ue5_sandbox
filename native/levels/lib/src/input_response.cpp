#include "ioj/input/response.h"
#include "ioj/vector_value_math.h"

#include <algorithm>
#include <cmath>

namespace ioj::input {
namespace {
auto inverted(Response const response, Axis const axis, GameSettingsState const& settings) noexcept
    -> bool {
    auto const pointer{response == Response::TurnPointerDelta};
    switch (axis) {
        case Axis::Pitch:
            return pointer ? settings.invert_mouse_pitch : settings.invert_gamepad_pitch;
        case Axis::Yaw:
            return pointer ? settings.invert_mouse_yaw : settings.invert_gamepad_yaw;
        case Axis::Roll:
            return !pointer && settings.invert_gamepad_roll;
        case Axis::VerticalTranslation:
            return !pointer && settings.invert_gamepad_vertical_translation;
        case Axis::None:
            return false;
    }
    return false;
}

auto scale(Response const response, GameSettingsState const& settings) noexcept -> double {
    switch (response) {
        case Response::TurnPointerDelta:
            return settings.mouse_turn_sensitivity / 400.0;
        case Response::GamepadTurn:
            return settings.gamepad_turn_sensitivity;
        case Response::GamepadMove:
            return 1.0;
    }
    return 1.0;
}

auto dead_zone(Response const response, GameSettingsState const& settings) noexcept -> double {
    return response == Response::GamepadTurn ? settings.gamepad_turn_dead_zone
                                             : settings.gamepad_move_dead_zone;
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
    return std::isfinite(value) ? std::max(0.0f, value) : 0.f;
}
auto clamp_dead_zone(float const value) noexcept -> float {
    return std::isfinite(value) ? std::clamp(value, 0.0f, 0.95f) : 0.f;
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
