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

using RoutedInput = std::variant<input::ActionTransition,
                                 host::MenuButton,
                                 host::MenuKey,
                                 host::MenuPointer,
                                 host::MenuAxis,
                                 host::MenuText>;

struct PhysicalInputState {
    std::vector<input::KeyCode> held;
    std::array<float, 6> gamepad_axes{};
    host::Modifiers modifiers{};
};

class GameInput {
  public:
    explicit GameInput(ControlsSettings const& controls);

    void synchronize(InputRoute route, host::MenuStatus menu);
    void refresh_bindings();
    void begin_frame();
    void resolve(input::PhysicalEvent const& event);
    void repeat_navigation(double seconds);
    auto relative_mouse() const -> bool;
    auto editing_text() const -> bool;
    auto state() const -> input::InputState const& { return state_; }
    void append_commands(std::vector<host::InputEvent>& output) const;
    void append_frame_commands(double seconds, std::vector<host::InputEvent>& output) const;
  private:
    struct RoutedButton {
        input::KeyCode key;
        input::DigitalAction action;
    };

    void button(input::KeyCode key, input::ButtonState edge, std::uint32_t character = {});
    void routed_button(input::KeyCode key, input::DigitalAction action, input::ButtonState edge);
    void publish_transitions(std::size_t first);
    void apply(input::PhysicalButton event);
    void apply(input::PhysicalPointerButton event);
    void apply(input::PhysicalAxis event);
    void apply(input::PointerMotion event);
    void apply(input::WheelMotion event);
    void apply(input::TextInput const& event);
    void apply(input::FocusLost);
    void apply(input::GamepadDisconnected);
    void release_context();

    ControlsSettings const& controls_;
    PhysicalInputState physical_;
    input::InputState state_;
    input::FlightInputResolver flight_;
    std::vector<RoutedInput> routed_;
    std::vector<RoutedButton> routed_buttons_;
    std::array<double, 8> repeat_delays_{};
    std::uint8_t pointer_capture_{};
    InputRoute route_{};
    input::MenuInputMode menu_mode_{};
    bool editing_text_{};
};
}
