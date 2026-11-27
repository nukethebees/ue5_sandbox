#pragma once

#include "player/input/physical/physical_input.h"
#include <ioj/input/dispatch/input_dispatcher.h>
#include <ioj/input/logical/flight_input.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace ioj {
struct PhysicalInputBatch;

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
    auto viewer_step(double seconds) const -> std::optional<host::ViewerStep>;
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
    void apply_button(InputDispatcher& output,
                      PhysicalKey key,
                      host::Modifiers event_modifiers,
                      ButtonState state,
                      bool repeat);
    void apply_pointer_button(InputDispatcher& output,
                              Vector2f position,
                              PhysicalKey key,
                              ButtonState state,
                              std::uint8_t clicks);
    void apply_axis(InputDispatcher& output, PhysicalKey axis_key, float axis_position);
    void apply_motion(InputDispatcher& output, Vector2f position, Vector2f delta);
    void apply_wheel(InputDispatcher& output, Vector2f position, float steps);
    void apply_text(InputDispatcher& output, std::string const& text);
    void apply_focus(InputDispatcher& output, WindowFocus focus);
    void apply_disconnect(InputDispatcher& output);
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
