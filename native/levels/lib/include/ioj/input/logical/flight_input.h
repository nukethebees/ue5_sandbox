#pragma once

#include <ioj/input/logical/flight_controls.h>
#include <ioj/input/logical/input_state.h>
#include <ioj/settings/controls_settings.h>

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

auto is_pause_button(FlightBindings const& bindings, PhysicalKey key) -> bool;

struct FlightInputState {
    void reset(LogicalInputState& state);
    void button(LogicalInputState& state,
                PhysicalKey key,
                ButtonState edge,
                ControlsSettings const& controls,
                InputContext context);
    void pointer(LogicalInputState& state, Vector2f delta, ControlsSettings const& controls);
    void axis(LogicalInputState& state,
              PhysicalKey key,
              float position,
              ControlsSettings const& controls);

    DigitalActions keyboard_mouse_held;
    DigitalActions gamepad_held;
    float keyboard_mouse_acceleration{};
    float gamepad_acceleration{};
    Vector2f gamepad_turn{};
    Vector2f keyboard_turn{};
};
}
