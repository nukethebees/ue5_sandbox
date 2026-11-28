#pragma once

#include "player/input/physical/physical_input_system.h"
#include "player/native_endpoint.h"
#include <ioj/input/logical/flight_input.h>
#include <ioj/sim/player/player_input_frame.h>

#include <array>
#include <cstddef>
#include <cstdint>
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

struct MenuButton {
    MenuCommand command;
    bool repeat;
};

struct InputCommand {
    enum class Kind { Action, Navigation, Capture };
    InputCommand(host::PlayerAction action)
        : kind{Kind::Action}
        , action{action} {}
    InputCommand(MenuButton button)
        : kind{Kind::Navigation}
        , button{button} {}
    InputCommand(PhysicalKey key)
        : kind{Kind::Capture}
        , key{key} {}

    Kind kind;
    union {
        host::PlayerAction action;
        MenuButton button;
        PhysicalKey key;
    };
};

class InputSystem {
  public:
    InputSystem(SDL_Window& window,
                ControlsSettings const& controls,
                host::NativeEndpoint& endpoint);
    auto poll() -> WindowInputEvents;
    auto events() const -> PhysicalInputBatch const& { return devices_.events(); }
    auto commands() const -> std::span<InputCommand const> { return commands_; }
    void clear_commands() { commands_.clear(); }
    auto keyboard() const -> KeyboardLayout const& { return devices_.keyboard(); }
    auto ship_input() -> sim::player::PlayerInputFrame;
    auto steering() const -> std::optional<Vector2d> {
        return state_.held.hold_to_steer() ? std::optional{state_.pointer_steering} : std::nullopt;
    }

    void synchronize(InputRoute route, MenuInputMode mode);
    void refresh_bindings();

    auto map_events(PhysicalInputBatch const& events, std::size_t first) -> std::size_t;
    void repeat_navigation(double seconds);
    auto relative_mouse() const -> bool;
    auto editing_text() const -> bool;
    auto state() const -> LogicalInputState const& { return state_; }
    void publish_viewer_step(double seconds);
  private:
    struct RoutedButton {
        PhysicalKey key;
        DigitalAction action;
    };

    void button(PhysicalKey key, ButtonState edge, bool repeat = false);
    void routed_button(PhysicalKey key, DigitalAction action, ButtonState edge);
    void publish_transitions(std::size_t first);
    void apply_button(PhysicalKey key, host::Modifiers modifiers, ButtonState state, bool repeat);
    void apply_pointer_button(Vector2f position,
                              PhysicalKey key,
                              ButtonState state,
                              std::uint8_t clicks);
    void apply_axis(PhysicalKey key, float position);
    void apply_motion(Vector2f position, Vector2f delta);
    void apply_wheel(Vector2f position, float steps);
    void apply_text(std::string const& text);
    void apply_focus(WindowFocus focus);
    void apply_disconnect();
    void release_context();

    ControlsSettings const& controls_;
    PhysicalInputSystem devices_;
    PhysicalInputState& physical_;
    host::NativeEndpoint& endpoint_;
    std::vector<InputCommand> commands_;
    std::vector<sim::player::PlayerInputKind> ship_events_;
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
