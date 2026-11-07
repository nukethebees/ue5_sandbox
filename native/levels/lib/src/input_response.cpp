#include "ioj/input/response.h"
#include "ioj/vector_value_math.h"

#include <algorithm>
#include <cassert>
#include <cmath>

namespace ioj::input {
ResponseSettings::ResponseSettings(GameSettingsState const& settings) noexcept
    : sensitivities_{std::max(0.f, settings.mouse_turn_sensitivity),
                     std::max(0.f, settings.gamepad_turn_sensitivity)}
    , dead_zones_{std::clamp(settings.gamepad_turn_dead_zone, 0.f, 0.95f),
                  std::clamp(settings.gamepad_move_dead_zone, 0.f, 0.95f)}
    , inverted_{{{false, settings.invert_mouse_pitch, settings.invert_mouse_yaw, false, false},
                 {false,
                  settings.invert_gamepad_pitch,
                  settings.invert_gamepad_yaw,
                  settings.invert_gamepad_roll,
                  settings.invert_gamepad_vertical_translation}}} {}

auto ResponseSettings::sensitivity(Response const response) const noexcept -> float {
    return sensitivities_[response == Response::TurnPointerDelta ? 0 : 1];
}
auto ResponseSettings::dead_zone(Response const response) const noexcept -> float {
    return dead_zones_[response == Response::GamepadTurn ? 0 : 1];
}
auto ResponseSettings::inverted(Response const response, Axis const axis) const noexcept -> bool {
    assert(static_cast<std::size_t>(axis) < inverted_.front().size());
    return inverted_[response == Response::TurnPointerDelta ? 0 : 1]
                    [static_cast<std::size_t>(axis)];
}

namespace {
auto scale(Response const response, ResponseSettings const& settings) noexcept -> double {
    switch (response) {
        case Response::TurnPointerDelta:
            return settings.sensitivity(response) / 400.0;
        case Response::GamepadTurn:
            return settings.sensitivity(response);
        case Response::GamepadMove:
            return 1.0;
    }
    return 1.0;
}

template <std::size_t N>
auto apply_vector_response(Vector<double, N> value,
                           Response const response,
                           Axis const axis,
                           ResponseSettings const& settings) noexcept -> Vector<double, N> {
    if (response != Response::TurnPointerDelta) {
        auto const threshold{static_cast<double>(settings.dead_zone(response))};
        auto const magnitude{size(value)};
        if (magnitude <= threshold) {
            return {};
        }
        auto const output_magnitude{std::min((magnitude - threshold) / (1.0 - threshold), 1.0)};
        value *= output_magnitude / magnitude;
    }
    value *= scale(response, settings);
    if (settings.inverted(response, axis)) {
        value.y() = -value.y();
    }
    return value;
}
}

auto apply_response(float value,
                    Response const response,
                    Axis const axis,
                    ResponseSettings const& settings) noexcept -> float {
    if (response != Response::TurnPointerDelta) {
        auto const threshold{static_cast<double>(settings.dead_zone(response))};
        auto const magnitude{std::abs(value)};
        value = magnitude <= threshold ? 0.0f
                                       : static_cast<float>(std::copysign(
                                             (magnitude - threshold) / (1.0 - threshold), value));
    }
    return static_cast<float>(value * scale(response, settings) *
                              (settings.inverted(response, axis) ? -1.0 : 1.0));
}
auto apply_response(Vector2d const value,
                    Response const response,
                    Axis const axis,
                    ResponseSettings const& settings) noexcept -> Vector2d {
    return apply_vector_response(value, response, axis, settings);
}
auto apply_response(Vector3d const value,
                    Response const response,
                    Axis const axis,
                    ResponseSettings const& settings) noexcept -> Vector3d {
    return apply_vector_response(value, response, axis, settings);
}
}
