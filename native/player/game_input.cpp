#include "player/game_input.h"

#include <algorithm>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <type_traits>
#include <utility>

namespace ioj {
namespace {
using input::ButtonState;
using input::Context;
using input::DigitalAction;
using input::KeyCode;
using input::MenuCommand;

}
namespace game_input_tables {
struct GamepadAxisKeys {
    PhysicalKey axis;
    PhysicalKey positive;
    PhysicalKey negative{PhysicalKey::Unbound};
};
constexpr std::array gamepad_axes{
    GamepadAxisKeys{PhysicalKey::GamepadLeftX,
                    PhysicalKey::GamepadLeftStickRight,
                    PhysicalKey::GamepadLeftStickLeft},
    GamepadAxisKeys{PhysicalKey::GamepadLeftY,
                    PhysicalKey::GamepadLeftStickUp,
                    PhysicalKey::GamepadLeftStickDown},
    GamepadAxisKeys{PhysicalKey::GamepadRightX,
                    PhysicalKey::GamepadRightStickRight,
                    PhysicalKey::GamepadRightStickLeft},
    GamepadAxisKeys{PhysicalKey::GamepadRightY,
                    PhysicalKey::GamepadRightStickUp,
                    PhysicalKey::GamepadRightStickDown},
    GamepadAxisKeys{PhysicalKey::GamepadLeftTriggerAxis, PhysicalKey::GamepadLeftTrigger},
    GamepadAxisKeys{PhysicalKey::GamepadRightTriggerAxis, PhysicalKey::GamepadRightTrigger},
};
constexpr std::array repeat_keys{PhysicalKey::GamepadDPadUp,
                                 PhysicalKey::GamepadDPadDown,
                                 PhysicalKey::GamepadDPadLeft,
                                 PhysicalKey::GamepadDPadRight,
                                 PhysicalKey::GamepadLeftStickRight,
                                 PhysicalKey::GamepadLeftStickLeft,
                                 PhysicalKey::GamepadLeftStickUp,
                                 PhysicalKey::GamepadLeftStickDown};
constexpr std::array menu_actions{DigitalAction::MenuUp,
                                  DigitalAction::MenuDown,
                                  DigitalAction::MenuLeft,
                                  DigitalAction::MenuRight,
                                  DigitalAction::MenuNext,
                                  DigitalAction::MenuPrevious,
                                  DigitalAction::MenuAccept,
                                  DigitalAction::MenuBack};
constexpr std::array pointer_keys{PhysicalKey::LeftMouseButton,
                                  PhysicalKey::RightMouseButton,
                                  PhysicalKey::MiddleMouseButton,
                                  PhysicalKey::ThumbMouseButton,
                                  PhysicalKey::ThumbMouseButton2};
constexpr EnumArray<float, input::ObserverSpeedPreset> observer_speeds{
    std::array{5000.f, 15000.f, 50000.f, 150000.f, 500000.f}};

}

namespace {
auto observer_action(PhysicalKey key) -> std::optional<DigitalAction> {
    switch (key) {
        case PhysicalKey::D:
            return DigitalAction::ObserverRight;
        case PhysicalKey::A:
            return DigitalAction::ObserverLeft;
        case PhysicalKey::W:
            return DigitalAction::ObserverForward;
        case PhysicalKey::S:
            return DigitalAction::ObserverBackward;
        case PhysicalKey::E:
            return DigitalAction::ObserverUp;
        case PhysicalKey::Q:
            return DigitalAction::ObserverDown;
        case PhysicalKey::LeftShift:
            return DigitalAction::ObserverFast;
        case PhysicalKey::RightMouseButton:
            return DigitalAction::ObserverToggleLook;
        default:
            return {};
    }
}
void track_button(PhysicalInputState& state, KeyCode key, ButtonState edge) {
    if (edge == ButtonState::Released) {
        std::erase(state.held, key);
    } else if (edge == ButtonState::Pressed && !std::ranges::contains(state.held, key)) {
        state.held.push_back(key);
    }
}
}

GameInput::GameInput(ControlsSettings const& controls)
    : controls_{controls} {
    refresh_bindings();
}
auto GameInput::relative_mouse() const -> bool {
    return route_.context == Context::Gameplay ||
           state_.observer_look == input::ObserverLookMode::Camera;
}
auto GameInput::editing_text() const -> bool {
    return route_.context == Context::Menu && editing_text_;
}
void GameInput::begin_frame() {
    state_.begin_frame();
    routed_.clear();
}
void GameInput::release_context() {
    for (auto const& button : routed_buttons_) {
        state_.button(button.action, ButtonState::Released);
    }
    routed_buttons_.clear();
    repeat_delays_ = {};
    state_.observer_look = input::ObserverLookMode::Pointer;
    state_.axes_2d[input::Axis2DAction::ObserverLookDelta] = {};
}
void GameInput::synchronize(InputRoute route, host::MenuStatus menu) {
    auto const mode{menu.capturing_binding || menu.editing_text ? input::MenuInputMode::Raw
                                                                : input::MenuInputMode::Navigation};
    editing_text_ = menu.editing_text;
    if (mode != menu_mode_) {
        menu_mode_ = mode;
        repeat_delays_ = {};
        if (route_.context == Context::Menu) {
            release_context();
        }
    }
    if (route != route_) {
        release_context();
        route_ = route;
        refresh_bindings();
    }
}
void GameInput::refresh_bindings() {
    flight_.configure(state_, controls_, route_.context);
    if (route_.context == Context::Gameplay) {
        for (std::size_t i{}; i < game_input_tables::gamepad_axes.size(); ++i) {
            auto const [axis, positive, negative]{game_input_tables::gamepad_axes[i]};
            auto const position{physical_.gamepad_axes[i]};
            flight_.restore_axis(state_, axis, position, controls_);
            flight_.restore_axis(state_, positive, position >= 0.5f ? 1.f : 0.f, controls_);
            if (negative != PhysicalKey::Unbound) {
                flight_.restore_axis(state_, negative, position <= -0.5f ? 1.f : 0.f, controls_);
            }
        }
    }
}
void GameInput::publish_transitions(std::size_t first) {
    for (auto const edge : std::span{state_.transitions}.subspan(first)) {
        routed_.emplace_back(edge);
    }
}
void GameInput::routed_button(KeyCode key, DigitalAction action, ButtonState edge) {
    auto found{std::ranges::find(routed_buttons_, key, &RoutedButton::key)};
    if (edge == ButtonState::Pressed && found != routed_buttons_.end() && found->action != action) {
        auto const previous{std::exchange(found->action, action)};
        if (!std::ranges::contains(routed_buttons_, previous, &RoutedButton::action)) {
            state_.button(previous, ButtonState::Released);
        }
    }
    if (edge == ButtonState::Released) {
        if (found == routed_buttons_.end()) {
            return;
        }
        action = found->action;
        routed_buttons_.erase(found);
        if (std::ranges::contains(routed_buttons_, action, &RoutedButton::action)) {
            return;
        }
    } else if (found == routed_buttons_.end() && edge == ButtonState::Pressed) {
        routed_buttons_.push_back({key, action});
    }
    auto const first{state_.transitions.size()};
    state_.button(action, edge);
    if (route_.context == Context::Benchmark) {
        publish_transitions(first);
    }
    if (action == DigitalAction::ObserverToggleLook && first != state_.transitions.size() &&
        edge == ButtonState::Pressed) {
        state_.observer_look = state_.observer_look == input::ObserverLookMode::Pointer
                                 ? input::ObserverLookMode::Camera
                                 : input::ObserverLookMode::Pointer;
    }
}
void GameInput::button(KeyCode key, ButtonState edge, std::uint32_t character) {
    auto const down{edge != ButtonState::Released};
    auto const repeat{edge == ButtonState::Repeated};
    if (route_.context == Context::Menu && menu_mode_ == input::MenuInputMode::Navigation &&
        !repeat) {
        auto const direction{std::ranges::find(game_input_tables::repeat_keys, key.key)};
        if (direction != game_input_tables::repeat_keys.end()) {
            repeat_delays_[direction - game_input_tables::repeat_keys.begin()] = down ? 0.4 : 0.0;
        }
    }
    if ((route_.resume_available || route_.context == Context::Observer ||
         route_.context == Context::Gameplay) &&
        (route_.context == Context::Gameplay || flight_.is_pause_button(key))) {
        auto const first{state_.transitions.size()};
        flight_.button(state_, key, edge, controls_);
        publish_transitions(first);
        return;
    }
    if (route_.context == Context::Menu && menu_mode_ == input::MenuInputMode::Raw) {
        routed_.emplace_back(host::MenuKey{key, character, down, repeat, physical_.modifiers});
        return;
    }
    auto const command{input::menu_command(
        key.key,
        route_.context == Context::Menu &&
            (physical_.modifiers.left_shift() || physical_.modifiers.right_shift()))};
    if (route_.context == Context::Menu && command) {
        routed_.emplace_back(host::MenuButton{*command, down, repeat, physical_.modifiers});
        routed_button(key, game_input_tables::menu_actions[std::to_underlying(*command)], edge);
        return;
    }
    if (edge == ButtonState::Released) {
        auto const found{std::ranges::find(routed_buttons_, key, &RoutedButton::key)};
        if (found != routed_buttons_.end()) {
            routed_button(key, found->action, edge);
        }
        return;
    }
    if (route_.context == Context::Observer) {
        if (auto action{observer_action(key.key)}; action && !repeat) {
            routed_button(key, *action, edge);
        }
        return;
    }
    if (route_.context == Context::Benchmark && !repeat) {
        if (command) {
            auto const action{*command == MenuCommand::Accept ? DigitalAction::BenchmarkActivate
                              : *command == MenuCommand::Back ? DigitalAction::BenchmarkEnd
                                                              : DigitalAction::BenchmarkNext};
            routed_button(key, action, edge);
        } else if (flight_.is_pause_button(key)) {
            routed_button(key, DigitalAction::BenchmarkToggle, edge);
        }
    }
}
void GameInput::resolve(input::PhysicalEvent const& event) {
    routed_.clear();
    std::visit([&](auto const& value) { apply(value); }, event);
}
void GameInput::apply(input::PhysicalButton event) {
    if (!is_gamepad_control(event.key.key)) {
        physical_.modifiers = event.modifiers;
    }
    track_button(physical_, event.key, event.state);
    button(event.key, event.state, event.character);
    auto const modifiers{physical_.modifiers};
    if (editing_text() && event.key == PhysicalKey::BackSpace &&
        event.state != ButtonState::Released && !modifiers.left_control() &&
        !modifiers.right_control() && !modifiers.left_alt() && !modifiers.right_alt()) {
        routed_.emplace_back(host::MenuText{"\b", modifiers});
    }
}
void GameInput::apply(input::TextInput const& event) {
    if (editing_text()) {
        routed_.emplace_back(host::MenuText{event.text, physical_.modifiers});
    }
}
void GameInput::apply(input::PhysicalPointerButton event) {
    track_button(physical_, event.key, event.state);
    auto const found{std::ranges::find(game_input_tables::pointer_keys, event.key)};
    assert(found != game_input_tables::pointer_keys.end());
    auto const mask{
        static_cast<std::uint8_t>(1u << (found - game_input_tables::pointer_keys.begin()))};
    auto const captured{(pointer_capture_ & mask) != 0};
    if ((route_.resume_available || route_.context == Context::Observer) &&
        flight_.is_pause_button(event.key) && !captured) {
        button(event.key, event.state);
        return;
    }
    auto const down{event.state == ButtonState::Pressed};
    auto const visible{!relative_mouse()};
    auto const camera_toggle{route_.context == Context::Observer &&
                             event.key == PhysicalKey::RightMouseButton};
    if ((down && visible && !camera_toggle) || (!down && captured)) {
        auto action{host::PointerAction::Up};
        if (down) {
            pointer_capture_ |= mask;
            action =
                event.clicks == 2 ? host::PointerAction::DoubleClick : host::PointerAction::Down;
        } else {
            pointer_capture_ &= ~mask;
        }
        routed_.emplace_back(host::MenuPointer{
            {action, event.key, event.position.x(), event.position.y(), 0.f}, physical_.modifiers});
    }
    if (route_.context != Context::Menu &&
        !(route_.context == Context::Benchmark && event.key == PhysicalKey::LeftMouseButton)) {
        button(event.key, event.state);
    }
}
void GameInput::apply(input::PointerMotion event) {
    if (route_.context == Context::Gameplay) {
        flight_.pointer_motion(state_, event.delta, controls_);
    } else if (state_.observer_look == input::ObserverLookMode::Camera) {
        auto& look{state_.axes_2d[input::Axis2DAction::ObserverLookDelta]};
        look.x() += event.delta.x();
        look.y() -= event.delta.y();
    } else {
        routed_.emplace_back(host::MenuPointer{{host::PointerAction::Move,
                                                PhysicalKey::Unbound,
                                                event.position.x(),
                                                event.position.y(),
                                                0.f},
                                               physical_.modifiers});
    }
}
void GameInput::apply(input::WheelMotion event) {
    if (route_.context == Context::Menu) {
        routed_.emplace_back(host::MenuPointer{{host::PointerAction::Wheel,
                                                PhysicalKey::Unbound,
                                                event.position.x(),
                                                event.position.y(),
                                                event.steps},
                                               physical_.modifiers});
    } else if (route_.context == Context::Observer) {
        auto const step{(event.steps > 0.f) - (event.steps < 0.f)};
        state_.observer_speed = static_cast<input::ObserverSpeedPreset>(
            std::clamp<int>(std::to_underlying(state_.observer_speed) + step,
                            std::to_underlying(enum_min<input::ObserverSpeedPreset>),
                            std::to_underlying(enum_max<input::ObserverSpeedPreset>)));
    } else if (route_.context == Context::Gameplay && event.steps != 0.f) {
        auto const key{event.steps > 0.f ? PhysicalKey::MouseScrollUp
                                         : PhysicalKey::MouseScrollDown};
        button(key, ButtonState::Pressed);
        button(key, ButtonState::Released);
    }
}
void GameInput::apply(input::PhysicalAxis event) {
    auto const axis{std::ranges::find(
        game_input_tables::gamepad_axes, event.key, &game_input_tables::GamepadAxisKeys::axis)};
    assert(axis != game_input_tables::gamepad_axes.end());
    auto& sample{physical_.gamepad_axes[axis - game_input_tables::gamepad_axes.begin()]};
    auto const before{std::exchange(sample, event.position)};
    if (route_.context == Context::Menu && menu_mode_ == input::MenuInputMode::Raw) {
        routed_.emplace_back(host::MenuAxis{event.key, sample, physical_.modifiers});
    } else if (route_.context == Context::Gameplay) {
        flight_.axis(state_, event.key, sample, controls_);
    }
    if ((before >= 0.5f) != (sample >= 0.5f)) {
        button(axis->positive, sample >= 0.5f ? ButtonState::Pressed : ButtonState::Released);
    }
    if (axis->negative != PhysicalKey::Unbound && (before <= -0.5f) != (sample <= -0.5f)) {
        button(axis->negative, sample <= -0.5f ? ButtonState::Pressed : ButtonState::Released);
    }
}
void GameInput::apply(input::GamepadDisconnected) {
    for (auto key : physical_.held) {
        if (is_gamepad_control(key.key)) {
            button(key, ButtonState::Released);
        }
    }
    std::erase_if(physical_.held, [](KeyCode key) { return is_gamepad_control(key.key); });
    for (std::size_t i{}; i < game_input_tables::gamepad_axes.size(); ++i) {
        if (physical_.gamepad_axes[i] != 0.f) {
            apply(input::PhysicalAxis{game_input_tables::gamepad_axes[i].axis, 0.f});
        }
    }
    repeat_delays_ = {};
}
void GameInput::apply(input::FocusLost) {
    apply(input::GamepadDisconnected{});
    physical_.held.clear();
    physical_.modifiers = {};
    pointer_capture_ = {};
    release_context();
    flight_.reset(state_);
}
void GameInput::repeat_navigation(double seconds) {
    routed_.clear();
    for (std::size_t i{}; i < game_input_tables::repeat_keys.size(); ++i) {
        auto& delay{repeat_delays_[i]};
        if (delay > 0.) {
            delay -= seconds;
            if (delay <= 0.) {
                button(game_input_tables::repeat_keys[i], ButtonState::Repeated);
                delay = 0.1;
            }
        }
    }
}
void GameInput::append_commands(std::vector<host::InputEvent>& output) const {
    for (auto const& record : routed_) {
        std::visit(
            [&](auto const& payload) {
                if constexpr (std::same_as<std::remove_cvref_t<decltype(payload)>,
                                           input::ActionTransition>) {
                    if (payload.state == ButtonState::Pressed) {
                        using enum host::PlayerAction;
                        switch (payload.action) {
                            case DigitalAction::Pause:
                                output.emplace_back(route_.resume_available ? RequestResume
                                                                            : OpenPause);
                                break;
                            case DigitalAction::BenchmarkNext:
                                output.emplace_back(BenchmarkNext);
                                break;
                            case DigitalAction::BenchmarkActivate:
                                output.emplace_back(BenchmarkActivate);
                                break;
                            case DigitalAction::BenchmarkToggle:
                                output.emplace_back(ToggleBenchmark);
                                break;
                            case DigitalAction::BenchmarkEnd:
                                output.emplace_back(EndBenchmark);
                                break;
                            default:
                                break;
                        }
                    }
                } else {
                    output.emplace_back(payload);
                }
            },
            record);
    }
}
void GameInput::append_frame_commands(double seconds, std::vector<host::InputEvent>& output) const {
    if (route_.context == Context::Gameplay) {
        output.emplace_back(host::SteeringIndicator{state_.pointer_steering});
    }
    if (route_.context == Context::Observer) {
        auto const& held{state_.held};
        auto const look{state_.axes_2d[input::Axis2DAction::ObserverLookDelta]};
        auto const speed{game_input_tables::observer_speeds[state_.observer_speed] *
                         (held.observer_fast() ? 4.f : 1.f)};
        output.emplace_back(
            host::ViewerStep{{float(held.observer_right() - held.observer_left()),
                              float(held.observer_forward() - held.observer_backward()),
                              float(held.observer_up() - held.observer_down()),
                              look.x() * 0.1f,
                              look.y() * 0.1f,
                              speed,
                              state_.observer_look == input::ObserverLookMode::Camera},
                             seconds});
    }
}
}
