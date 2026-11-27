#pragma once

#include "player/physical_input.h"
#include <ioj/host/input.h>
#include <ioj/input/flight_input.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <variant>
#include <vector>

namespace ioj {
struct InputRoute {
    input::Context context{};
    bool resume_available{};
    bool operator==(InputRoute const&) const = default;
};

struct PhysicalInputState {
    std::vector<input::KeyCode> held;
    std::array<float, 6> gamepad_axes{};
    host::Modifiers modifiers{};
    input::WindowFocus focus{input::WindowFocus::Gained};
};

class GameInput {
  public:
    GameInput(ControlsSettings const& controls, input::WindowFocus focus);

    void
        synchronize(host::InputEvents& output, InputRoute route, host::MenuStatus menu);
    void refresh_bindings(host::InputEvents& output);
    void begin_frame();
    void resolve(host::InputEvents& output, input::PhysicalEvent const& event);
    void repeat_navigation(host::InputEvents& output, double seconds);
    auto relative_mouse() const -> bool;
    auto editing_text() const -> bool;
    auto state() const -> input::InputState const& { return state_; }
    void append_frame_commands(double seconds, host::InputEvents& output) const;
  private:
    struct RoutedButton {
        input::KeyCode key;
        input::DigitalAction action;
    };

    void button(host::InputEvents& output,
                input::KeyCode key,
                input::ButtonState edge,
                std::uint32_t character = {});
    void routed_button(host::InputEvents& output,
                       input::KeyCode key,
                       input::DigitalAction action,
                       input::ButtonState edge);
    void publish_transitions(host::InputEvents& output, std::size_t first);
    void apply(host::InputEvents& output, input::PhysicalButton event);
    void apply(host::InputEvents& output, input::PhysicalPointerButton event);
    void apply(host::InputEvents& output, input::PhysicalAxis event);
    void apply(host::InputEvents& output, input::PointerMotion event);
    void apply(host::InputEvents& output, input::WheelMotion event);
    void apply(host::InputEvents& output, input::TextInput const& event);
    void apply(host::InputEvents& output, input::WindowFocus focus);
    void apply(host::InputEvents& output, input::GamepadDisconnected);
    void release_context(host::InputEvents& output);

    ControlsSettings const& controls_;
    PhysicalInputState physical_;
    input::InputState state_;
    input::FlightInputResolver flight_;
    std::vector<RoutedButton> routed_buttons_;
    std::array<double, 4> repeat_delays_{};
    std::uint8_t pointer_capture_{};
    InputRoute route_{};
    input::MenuInputMode menu_mode_{};
    bool editing_text_{};
};
}
