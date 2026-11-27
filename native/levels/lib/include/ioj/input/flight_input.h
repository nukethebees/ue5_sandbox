#pragma once

#include <ioj/input/flight_controls.h>
#include <ioj/input/input_state.h>

#include <array>
#include <cstdint>

namespace ioj {
class FlightBindings {
  public:
    FlightBindings() = default;
    FlightBindings(ControlsSettings const& controls, InputContext context);
    auto matches(FlightMapping mapping, KeyCode key) const -> bool;
  private:
    EnumArray<std::uint32_t, FlightMapping> bound_characters_;
    EnumArray<PhysicalKey, FlightMapping> bound_keys_;
};

class FlightInputResolver {
  public:
    void configure(InputState& state, ControlsSettings const& controls, InputContext context);
    auto is_pause_button(KeyCode key) const -> bool;
    void button(InputState& state, KeyCode key, ButtonState edge, ControlsSettings const& controls);
    void pointer_motion(InputState& state, Vector2f delta, ControlsSettings const& controls);
    void sample_axis(InputState& state,
                     KeyCode key,
                     float position,
                     ControlsSettings const& controls);
    void reset(InputState& state);
  private:
    void update_turn(InputState& state, ControlsSettings const& controls) const;
    void resolve_digital(InputState& state, KeyCode key, bool down);

    FlightBindings bindings_;
    DigitalActions keyboard_mouse_held_;
    DigitalActions gamepad_held_;
    std::array<float, 2> accelerator_sources_{};
    Vector2f gamepad_turn_{};
    Vector2f keyboard_turn_{};
};
}
