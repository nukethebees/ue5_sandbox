#pragma once

#include "ioj/settings/game_settings.h"
#include "ioj/vector.h"

namespace ioj::input {
enum class Response { TurnPointerDelta, GamepadTurn, GamepadMove };
enum class Axis { None, Pitch, Yaw, Roll, VerticalTranslation };

auto clamp_sensitivity(float value) noexcept -> float;
auto clamp_dead_zone(float value) noexcept -> float;

auto apply_response(float value,
                    Response response,
                    Axis axis,
                    GameSettingsState const& settings) noexcept -> float;
auto apply_response(Vector2d value,
                    Response response,
                    Axis axis,
                    GameSettingsState const& settings) noexcept -> Vector2d;
auto apply_response(Vector3d value,
                    Response response,
                    Axis axis,
                    GameSettingsState const& settings) noexcept -> Vector3d;
}
