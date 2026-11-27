#pragma once

#include <ioj/input/logical/flight_controls.h>
#include <ioj/input/logical/input_state.h>
#include <ioj/settings/controls_settings.h>

#include <array>

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

class FlightInputResolver {
  public:
    void configure(InputState& state, ControlsSettings const& controls, InputContext context);
    auto is_pause_button(PhysicalKey key) const -> bool;
    void button(InputState& state,
                PhysicalKey key,
                ButtonState edge,
                ControlsSettings const& controls);
    void pointer_motion(InputState& state, Vector2f delta, ControlsSettings const& controls);
    void sample_axis(InputState& state,
                     PhysicalKey key,
                     float position,
                     ControlsSettings const& controls);
    void reset(InputState& state);
  private:
    void update_turn(InputState& state, ControlsSettings const& controls) const;
    void resolve_digital(InputState& state, PhysicalKey key, bool down);

    FlightBindings bindings_;
    DigitalActions keyboard_mouse_held_;
    DigitalActions gamepad_held_;
    std::array<float, 2> accelerator_sources_{};
    Vector2f gamepad_turn_{};
    Vector2f keyboard_turn_{};
};
}
