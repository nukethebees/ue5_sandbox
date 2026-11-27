#pragma once

#include "player/input/physical/physical_input.h"
#include <ioj/input/dispatch/events.h>
#include <ioj/input/logical/flight_input.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <variant>
#include <vector>

namespace ioj {
struct InputRoute {
    InputContext context{};
    bool resume_available{};
    bool operator==(InputRoute const&) const = default;
};

struct ObserverInputState {
    ObserverSpeedPreset speed{ObserverSpeedPreset::Normal};
    ObserverLookMode look{ObserverLookMode::Pointer};
};

class GameInput {
  public:
    GameInput(ControlsSettings const& controls, WindowFocus focus);

    void synchronize(host::InputEvents& output, InputRoute route, MenuInputMode mode);
    void refresh_bindings(host::InputEvents& output);
    void begin_frame();
    void resolve(host::InputEvents& output, PhysicalEvent const& event);
    void repeat_navigation(host::InputEvents& output, double seconds);
    auto relative_mouse() const -> bool;
    auto editing_text() const -> bool;
    auto state() const -> LogicalInputState const& { return state_; }
    void append_frame_commands(double seconds, host::InputEvents& output) const;
  private:
    struct RoutedButton {
        PhysicalKey key;
        DigitalAction action;
    };

    void button(host::InputEvents& output, PhysicalKey key, ButtonState edge, bool repeat = false);
    void routed_button(host::InputEvents& output,
                       PhysicalKey key,
                       DigitalAction action,
                       ButtonState edge);
    void publish_transitions(host::InputEvents& output, std::size_t first);
    void apply(host::InputEvents& output, PhysicalButton event);
    void apply(host::InputEvents& output, PhysicalPointerButton event);
    void apply(host::InputEvents& output, PhysicalAxis event);
    void apply(host::InputEvents& output, PointerMotion event);
    void apply(host::InputEvents& output, WheelMotion event);
    void apply(host::InputEvents& output, TextInput const& event);
    void apply(host::InputEvents& output, WindowFocus focus);
    void apply(host::InputEvents& output, GamepadDisconnected);
    void release_context(host::InputEvents& output);

    ControlsSettings const& controls_;
    PhysicalInputState physical_;
    LogicalInputState state_;
    FlightInputState flight_;
    ObserverInputState observer_;
    std::vector<RoutedButton> routed_buttons_;
    std::array<double, digital_action_repeatable_navigation_values.size()> repeat_delays_{};
    std::uint8_t pointer_capture_{};
    InputRoute route_{};
    MenuInputMode menu_mode_{};
};
}
