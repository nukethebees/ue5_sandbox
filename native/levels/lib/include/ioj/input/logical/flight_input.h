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

struct FlightInputState {
    DigitalActions keyboard_mouse_held;
    DigitalActions gamepad_held;
    float keyboard_mouse_acceleration{};
    float gamepad_acceleration{};
    Vector2f gamepad_turn{};
    Vector2f keyboard_turn{};
};
auto is_pause_button(FlightBindings const& bindings, PhysicalKey key) -> bool;
void reset_flight_input(FlightInputState& flight, LogicalInputState& state);
void map_flight_button(FlightInputState& flight,
                       LogicalInputState& state,
                       PhysicalKey key,
                       ButtonState edge,
                       ControlsSettings const& controls,
                       InputContext context);
void map_flight_pointer(FlightInputState& flight,
                        LogicalInputState& state,
                        Vector2f delta,
                        ControlsSettings const& controls);
void map_flight_axis(FlightInputState& flight,
                     LogicalInputState& state,
                     PhysicalKey key,
                     float position,
                     ControlsSettings const& controls);
}
