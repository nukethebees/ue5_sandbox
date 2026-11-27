#pragma once

#include <ioj/input/actions.h>
#include <ioj/settings/controls_settings.h>
#include <ioj/vector.h>

namespace ioj {

auto clamp_sensitivity(float value) noexcept -> float;
auto clamp_dead_zone(float value) noexcept -> float;

auto apply_response(float value,
                    InputResponse response,
                    FlightAxis axis,
                    ControlsSettings const& settings) noexcept -> float;
auto apply_response(Vector2d value,
                    InputResponse response,
                    FlightAxis axis,
                    ControlsSettings const& settings) noexcept -> Vector2d;
auto apply_response(Vector3d value,
                    InputResponse response,
                    FlightAxis axis,
                    ControlsSettings const& settings) noexcept -> Vector3d;
}
