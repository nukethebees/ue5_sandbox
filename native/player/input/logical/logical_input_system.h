#pragma once

#include "player/input/physical/physical_input.h"
#include <ioj/input/dispatch/input_dispatcher.h>
#include <ioj/input/logical/flight_input.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace ioj {
class PhysicalInputBatch;

struct InputRoute {
    InputContext context{};
    bool resume_available{};
    bool operator==(InputRoute const&) const = default;
};

struct ObserverInputState {
    ObserverSpeedPreset speed{ObserverSpeedPreset::Normal};
    ObserverLookMode look{ObserverLookMode::Pointer};
};

class LogicalInputSystem {
  public:
    LogicalInputSystem(ControlsSettings const& controls, PhysicalInputState& physical);

    void synchronize(InputDispatcher& output, InputRoute route, MenuInputMode mode);
    void refresh_bindings(InputDispatcher& output);
    void begin_frame();
    auto map_until_dispatch(InputDispatcher& output,
                            PhysicalInputBatch const& events,
                            std::size_t first) -> std::size_t;
    void repeat_navigation(InputDispatcher& output, double seconds);
    auto relative_mouse() const -> bool;
    auto editing_text() const -> bool;
    auto state() const -> LogicalInputState const& { return state_; }
    void append_frame_commands(double seconds, InputDispatcher& output) const;
  private:
    struct RoutedButton {
        PhysicalKey key;
        DigitalAction action;
    };

    void button(InputDispatcher& output, PhysicalKey key, ButtonState edge, bool repeat = false);
    void routed_button(InputDispatcher& output,
                       PhysicalKey key,
                       DigitalAction action,
                       ButtonState edge);
    void publish_transitions(InputDispatcher& output, std::size_t first);
    void apply(InputDispatcher& output, PhysicalButton event);
    void apply(InputDispatcher& output, PhysicalPointerButton event);
    void apply(InputDispatcher& output, PhysicalAxis event);
    void apply(InputDispatcher& output, PointerMotion event);
    void apply(InputDispatcher& output, WheelMotion event);
    void apply(InputDispatcher& output, TextInput const& event);
    void apply(InputDispatcher& output, WindowFocus focus);
    void apply(InputDispatcher& output, GamepadDisconnected);
    void release_context(InputDispatcher& output);

    ControlsSettings const& controls_;
    PhysicalInputState& physical_;
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
