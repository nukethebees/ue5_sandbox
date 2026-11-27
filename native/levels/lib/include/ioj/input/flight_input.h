#pragma once

#include <ioj/input/actions.h>
#include <ioj/input/flight_mapping.h>
#include <ioj/input/key_code.h>
#include <ioj/settings/controls_settings.h>
#include <ioj/sim/player/player_events.h>
#include <ioj/vector.h>

#include <cstdint>
#include <memory_resource>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ioj::input {
enum class FlightAction {
    Pause,
    Accelerate,
    Boost,
    Brake,
    EmergencyBrake,
    Fire,
    Steer,
    PointerPitch,
    PointerYaw,
    Pitch,
    Yaw
};

struct FlightControl {
    FlightMapping mapping;
    FlightAction action;
    KeyCode default_key;
    std::string_view label;
    int display_order;
};
auto accepts_binding(FlightMapping mapping, KeyCode key) -> bool;
auto flight_controls() -> std::span<FlightControl const>;
auto bound_key(FlightControl const& control, ControlsSettings const& settings) -> KeyCode;

struct FlightBinding {
    FlightAction action;
    KeyCode key;
    float value{};
};

class FlightInput {
  public:
    FlightInput();

    void configure(ControlsSettings const& controls, Context context);
    auto is_pause_button(KeyCode key) const -> bool;
    void restore_axis(KeyCode key, float position, ControlsSettings const& controls);
    // Returns true on the rising edge of the pause action.
    auto update(KeyCode key, float next, ControlsSettings const& controls) -> bool;
    void reset();
    void begin_frame();
    auto frame() const -> sim::player::PlayerInputFrame { return events_.view(); }
    auto steering() const -> std::optional<Vector2d> { return steering_; }
  private:
    auto value(FlightAction action) const -> float;
    void update_axes(ControlsSettings const& controls);

    std::vector<FlightBinding> bindings_;
    sim::player::PlayerInputEvents events_;
    std::optional<Vector2d> steering_;
};
}
