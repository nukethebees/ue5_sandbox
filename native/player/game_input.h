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
};

class GameInput {
  public:
    explicit GameInput(ControlsSettings const& controls);

    void
        synchronize(std::vector<host::InputEvent>& output, InputRoute route, host::MenuStatus menu);
    void refresh_bindings(std::vector<host::InputEvent>& output);
    void begin_frame();
    void resolve(std::vector<host::InputEvent>& output, input::PhysicalEvent const& event);
    void repeat_navigation(std::vector<host::InputEvent>& output, double seconds);
    auto relative_mouse() const -> bool;
    auto editing_text() const -> bool;
    auto state() const -> input::InputState const& { return state_; }
    void append_frame_commands(double seconds, std::vector<host::InputEvent>& output) const;
  private:
    struct RoutedButton {
        input::KeyCode key;
        input::DigitalAction action;
    };

    void button(std::vector<host::InputEvent>& output,
                input::KeyCode key,
                input::ButtonState edge,
                std::uint32_t character = {});
    void routed_button(std::vector<host::InputEvent>& output,
                       input::KeyCode key,
                       input::DigitalAction action,
                       input::ButtonState edge);
    void publish_transitions(std::vector<host::InputEvent>& output, std::size_t first);
    void apply(std::vector<host::InputEvent>& output, input::PhysicalButton event);
    void apply(std::vector<host::InputEvent>& output, input::PhysicalPointerButton event);
    void apply(std::vector<host::InputEvent>& output, input::PhysicalAxis event);
    void apply(std::vector<host::InputEvent>& output, input::PointerMotion event);
    void apply(std::vector<host::InputEvent>& output, input::WheelMotion event);
    void apply(std::vector<host::InputEvent>& output, input::TextInput const& event);
    void apply(std::vector<host::InputEvent>& output, input::FocusLost);
    void apply(std::vector<host::InputEvent>& output, input::GamepadDisconnected);
    void release_context(std::vector<host::InputEvent>& output);

    ControlsSettings const& controls_;
    PhysicalInputState physical_;
    input::InputState state_;
    input::FlightInputResolver flight_;
    std::vector<RoutedButton> routed_buttons_;
    std::array<double, 8> repeat_delays_{};
    std::uint8_t pointer_capture_{};
    InputRoute route_{};
    input::MenuInputMode menu_mode_{};
    bool editing_text_{};
};
}
