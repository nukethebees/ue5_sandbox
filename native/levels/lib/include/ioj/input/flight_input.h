#pragma once

#include <ioj/input/flight_controls.h>
#include <ioj/input/input_state.h>
#include <ioj/settings/controls_settings.h>

#include <array>

namespace ioj {
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
