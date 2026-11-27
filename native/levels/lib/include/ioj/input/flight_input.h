#pragma once

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
enum class Context { Menu, Gameplay, BenchmarkViewer };
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
    std::string_view mapping;
    FlightAction action;
    std::string_view default_key;
    std::string_view label;
    int display_order;
    bool gamepad;
};
auto binding_name_equal(std::string_view left, std::string_view right) -> bool;
auto flight_controls() -> std::span<FlightControl const>;
auto bound_key(FlightControl const& control, ControlsSettings const& settings) -> std::string_view;

struct FlightBinding {
    FlightAction action;
    std::string key;
    float value{};
};

class FlightInput {
  public:
    FlightInput();

    void configure(ControlsSettings const& controls, Context context);
    // Returns true on the rising edge of the pause action.
    auto update(std::string_view key, float next, ControlsSettings const& controls) -> bool;
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
