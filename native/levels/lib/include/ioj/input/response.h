#pragma once

#include "ioj/vector.h"
#include <ioj/settings/game_settings_state.h>

#include <array>

namespace ioj::input {
enum class Response { TurnPointerDelta, GamepadTurn, GamepadMove };
enum class Axis { None, Pitch, Yaw, Roll, VerticalTranslation };

class ResponseSettings {
  public:
    ResponseSettings() = default;
    explicit ResponseSettings(GameSettingsState const& settings) noexcept;

    [[nodiscard]] auto sensitivity(Response response) const noexcept -> float;
    [[nodiscard]] auto dead_zone(Response response) const noexcept -> float;
    [[nodiscard]] auto inverted(Response response, Axis axis) const noexcept -> bool;

  private:
    std::array<float, 2> sensitivities_{};
    std::array<float, 2> dead_zones_{};
    std::array<std::array<bool, 5>, 2> inverted_{};
};

auto apply_response(float value,
                    Response response,
                    Axis axis,
                    ResponseSettings const& settings) noexcept -> float;
auto apply_response(Vector2d value,
                    Response response,
                    Axis axis,
                    ResponseSettings const& settings) noexcept -> Vector2d;
auto apply_response(Vector3d value,
                    Response response,
                    Axis axis,
                    ResponseSettings const& settings) noexcept -> Vector3d;
}
