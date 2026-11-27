#include "player/game_input.h"

#include <algorithm>
#include <cassert>
#include <cmath>
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
constexpr std::array repeating_actions{DigitalAction::MenuUp,
                                       DigitalAction::MenuDown,
                                       DigitalAction::MenuLeft,
                                       DigitalAction::MenuRight};
constexpr std::array pointer_keys{PhysicalKey::LeftMouseButton,
                                  PhysicalKey::RightMouseButton,
                                  PhysicalKey::MiddleMouseButton,
                                  PhysicalKey::ThumbMouseButton,
                                  PhysicalKey::ThumbMouseButton2};
constexpr EnumArray<float, input::ObserverSpeedPreset> observer_speeds{
    std::array{5000.f, 15000.f, 50000.f, 150000.f, 500000.f}};

}

namespace {
auto menu_action(PhysicalKey key, bool shift) -> std::optional<DigitalAction> {
    switch (key) {
        case PhysicalKey::Up:
        case PhysicalKey::GamepadDPadUp:
        case PhysicalKey::GamepadLeftStickUp:
            return DigitalAction::MenuUp;
        case PhysicalKey::Down:
        case PhysicalKey::GamepadDPadDown:
        case PhysicalKey::GamepadLeftStickDown:
            return DigitalAction::MenuDown;
        case PhysicalKey::Left:
        case PhysicalKey::GamepadDPadLeft:
        case PhysicalKey::GamepadLeftStickLeft:
            return DigitalAction::MenuLeft;
        case PhysicalKey::Right:
        case PhysicalKey::GamepadDPadRight:
        case PhysicalKey::GamepadLeftStickRight:
            return DigitalAction::MenuRight;
        case PhysicalKey::Tab:
            return shift ? DigitalAction::MenuPrevious : DigitalAction::MenuNext;
        case PhysicalKey::Enter:
        case PhysicalKey::SpaceBar:
        case PhysicalKey::GamepadFaceButtonBottom:
            return DigitalAction::MenuAccept;
        case PhysicalKey::Escape:
        case PhysicalKey::GamepadFaceButtonRight:
            return DigitalAction::MenuBack;
        default:
            return {};
    }
}

auto menu_command(DigitalAction action) -> std::optional<MenuCommand> {
    switch (action) {
        case DigitalAction::MenuUp:
            return MenuCommand::Up;
        case DigitalAction::MenuDown:
            return MenuCommand::Down;
        case DigitalAction::MenuLeft:
            return MenuCommand::Left;
        case DigitalAction::MenuRight:
            return MenuCommand::Right;
        case DigitalAction::MenuNext:
            return MenuCommand::Next;
        case DigitalAction::MenuPrevious:
            return MenuCommand::Previous;
        case DigitalAction::MenuAccept:
            return MenuCommand::Accept;
        case DigitalAction::MenuBack:
            return MenuCommand::Back;
        default:
            return {};
    }
}
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
auto track_button(PhysicalInputState& state, KeyCode key, ButtonState edge) -> bool {
    if (edge == ButtonState::Released) {
        return std::erase(state.held, key) != 0;
    } else if (edge == ButtonState::Pressed && !std::ranges::contains(state.held, key)) {
        state.held.push_back(key);
        return true;
    }
    return edge == ButtonState::Repeated && std::ranges::contains(state.held, key);
}
auto axis_edge(PhysicalInputState& state, KeyCode key, float magnitude)
    -> std::optional<ButtonState> {
    auto const held{std::ranges::contains(state.held, key)};
    auto const active{held ? magnitude > 0.40f : magnitude >= 0.50f};
    if (held == active) {
        return {};
    }
    auto const edge{active ? ButtonState::Pressed : ButtonState::Released};
    track_button(state, key, edge);
    return edge;
}
}

GameInput::GameInput(ControlsSettings const& controls, input::WindowFocus focus)
    : controls_{controls} {
    physical_.focus = focus;
    flight_.configure(state_, controls_, route_.context);
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
}
void GameInput::release_context(std::vector<host::InputEvent>& output) {
    auto const first{state_.transitions.size()};
    for (auto const& button : routed_buttons_) {
        state_.button(button.action, ButtonState::Released);
    }
    publish_transitions(output, first);
    routed_buttons_.clear();
    repeat_delays_ = {};
    state_.observer_look = input::ObserverLookMode::Pointer;
    state_.axes_2d[input::Axis2DAction::ObserverLookDelta] = {};
}
void GameInput::synchronize(std::vector<host::InputEvent>& output,
                            InputRoute route,
                            host::MenuStatus menu) {
    auto const mode{menu.capturing_binding || menu.editing_text ? input::MenuInputMode::Raw
                                                                : input::MenuInputMode::Navigation};
    editing_text_ = menu.editing_text;
    if (mode != menu_mode_) {
        menu_mode_ = mode;
        repeat_delays_ = {};
        if (route_.context == Context::Menu) {
            release_context(output);
        }
    }
    if (route != route_) {
        release_context(output);
        route_ = route;
        refresh_bindings(output);
    }
}
void GameInput::refresh_bindings(std::vector<host::InputEvent>& output) {
    auto const first{state_.transitions.size()};
    flight_.configure(state_, controls_, route_.context);
    publish_transitions(output, first);
    if (route_.context == Context::Gameplay && physical_.focus == input::WindowFocus::Gained) {
        for (auto key : physical_.held) {
            flight_.sample_axis(state_, key, 1.f, controls_);
        }
        for (std::size_t i{}; i < game_input_tables::gamepad_axes.size(); ++i) {
            flight_.sample_axis(state_,
                                game_input_tables::gamepad_axes[i].axis,
                                physical_.gamepad_axes[i],
                                controls_);
        }
    }
}
void GameInput::publish_transitions(std::vector<host::InputEvent>& output, std::size_t first) {
    for (auto const payload : std::span{state_.transitions}.subspan(first)) {
        if (auto command{menu_command(payload.action)}) {
            output.emplace_back(host::MenuButton{*command,
                                                 payload.state != ButtonState::Released,
                                                 payload.state == ButtonState::Repeated,
                                                 physical_.modifiers});
            continue;
        }
        if (payload.state == ButtonState::Pressed) {
            using enum host::PlayerAction;
            switch (payload.action) {
                case DigitalAction::Pause:
                    output.emplace_back(route_.resume_available ? RequestResume : OpenPause);
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
    }
}
void GameInput::routed_button(std::vector<host::InputEvent>& output,
                              KeyCode key,
                              DigitalAction action,
                              ButtonState edge) {
    auto found{std::ranges::find(routed_buttons_, key, &RoutedButton::key)};
    if (edge == ButtonState::Pressed) {
        if (found != routed_buttons_.end()) {
            return;
        }
        routed_buttons_.push_back({key, action});
    } else {
        if (found == routed_buttons_.end()) {
            return;
        }
        action = found->action;
        if (edge == ButtonState::Released) {
            routed_buttons_.erase(found);
        }
    }
    auto const direction{std::ranges::find(game_input_tables::repeating_actions, action)};
    if (direction != game_input_tables::repeating_actions.end() && is_gamepad_control(key.key)) {
        auto& delay{repeat_delays_[direction - game_input_tables::repeating_actions.begin()]};
        if (edge == ButtonState::Pressed && delay == 0.) {
            delay = 0.4;
        } else if (edge == ButtonState::Released &&
                   !std::ranges::any_of(routed_buttons_, [action](auto source) {
                       return source.action == action && is_gamepad_control(source.key.key);
                   })) {
            delay = 0.;
        }
    }
    if (edge == ButtonState::Released &&
        std::ranges::contains(routed_buttons_, action, &RoutedButton::action)) {
        return;
    }
    auto const first{state_.transitions.size()};
    state_.button(action, edge);
    publish_transitions(output, first);
    if (action == DigitalAction::ObserverToggleLook && first != state_.transitions.size() &&
        edge == ButtonState::Pressed) {
        state_.observer_look = state_.observer_look == input::ObserverLookMode::Pointer
                                 ? input::ObserverLookMode::Camera
                                 : input::ObserverLookMode::Pointer;
    }
}
void GameInput::button(std::vector<host::InputEvent>& output,
                       KeyCode key,
                       ButtonState edge,
                       std::uint32_t character) {
    auto const down{edge != ButtonState::Released};
    auto const repeat{edge == ButtonState::Repeated};
    if ((route_.resume_available || route_.context == Context::Observer ||
         route_.context == Context::Gameplay) &&
        (route_.context == Context::Gameplay || flight_.is_pause_button(key))) {
        auto const first{state_.transitions.size()};
        flight_.button(state_, key, edge, controls_);
        publish_transitions(output, first);
        return;
    }
    if (route_.context == Context::Menu && menu_mode_ == input::MenuInputMode::Raw) {
        output.emplace_back(host::MenuKey{key, character, down, repeat, physical_.modifiers});
        return;
    }
    if (edge != ButtonState::Pressed) {
        auto const source{std::ranges::find(routed_buttons_, key, &RoutedButton::key)};
        if (source != routed_buttons_.end() && (!repeat || route_.context == Context::Menu)) {
            routed_button(output, key, source->action, edge);
        }
        return;
    }
    auto const navigation{
        menu_action(key.key,
                    route_.context == Context::Menu &&
                        (physical_.modifiers.left_shift() || physical_.modifiers.right_shift()))};
    if (route_.context == Context::Menu && navigation) {
        routed_button(output, key, *navigation, edge);
        return;
    }
    if (route_.context == Context::Observer) {
        if (auto action{observer_action(key.key)}; action && !repeat) {
            routed_button(output, key, *action, edge);
        }
        return;
    }
    if (route_.context == Context::Benchmark && !repeat) {
        if (navigation) {
            auto const action{
                *navigation == DigitalAction::MenuAccept ? DigitalAction::BenchmarkActivate
                : *navigation == DigitalAction::MenuBack ? DigitalAction::BenchmarkEnd
                                                         : DigitalAction::BenchmarkNext};
            routed_button(output, key, action, edge);
        } else if (flight_.is_pause_button(key)) {
            routed_button(output, key, DigitalAction::BenchmarkToggle, edge);
        }
    }
}
void GameInput::resolve(std::vector<host::InputEvent>& output, input::PhysicalEvent const& event) {
    std::visit([&](auto const& value) { apply(output, value); }, event);
}
void GameInput::apply(std::vector<host::InputEvent>& output, input::PhysicalButton event) {
    if (!is_gamepad_control(event.key.key)) {
        physical_.modifiers = event.modifiers;
    }
    if (!track_button(physical_, event.key, event.state) ||
        physical_.focus == input::WindowFocus::Lost) {
        return;
    }
    button(output, event.key, event.state, event.character);
    auto const modifiers{physical_.modifiers};
    if (editing_text() && event.key == PhysicalKey::BackSpace &&
        event.state != ButtonState::Released && !modifiers.left_control() &&
        !modifiers.right_control() && !modifiers.left_alt() && !modifiers.right_alt()) {
        output.emplace_back(host::MenuText{"\b", modifiers});
    }
}
void GameInput::apply(std::vector<host::InputEvent>& output, input::TextInput const& event) {
    if (physical_.focus == input::WindowFocus::Gained && editing_text()) {
        output.emplace_back(host::MenuText{event.text, physical_.modifiers});
    }
}
void GameInput::apply(std::vector<host::InputEvent>& output, input::PhysicalPointerButton event) {
    if (!track_button(physical_, event.key, event.state) ||
        physical_.focus == input::WindowFocus::Lost) {
        return;
    }
    auto const found{std::ranges::find(game_input_tables::pointer_keys, event.key)};
    assert(found != game_input_tables::pointer_keys.end());
    auto const mask{
        static_cast<std::uint8_t>(1u << (found - game_input_tables::pointer_keys.begin()))};
    auto const captured{(pointer_capture_ & mask) != 0};
    if ((route_.resume_available || route_.context == Context::Observer) &&
        flight_.is_pause_button(event.key) && !captured) {
        button(output, event.key, event.state);
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
        output.emplace_back(host::MenuPointer{
            {action, event.key, event.position.x(), event.position.y(), 0.f}, physical_.modifiers});
    }
    if (route_.context != Context::Menu &&
        !(route_.context == Context::Benchmark && event.key == PhysicalKey::LeftMouseButton)) {
        button(output, event.key, event.state);
    }
}
void GameInput::apply(std::vector<host::InputEvent>& output, input::PointerMotion event) {
    if (physical_.focus == input::WindowFocus::Lost) {
        return;
    }
    if (route_.context == Context::Gameplay) {
        flight_.pointer_motion(state_, event.delta, controls_);
    } else if (state_.observer_look == input::ObserverLookMode::Camera) {
        auto& look{state_.axes_2d[input::Axis2DAction::ObserverLookDelta]};
        look.x() += event.delta.x();
        look.y() -= event.delta.y();
    } else {
        output.emplace_back(host::MenuPointer{{host::PointerAction::Move,
                                               PhysicalKey::Unbound,
                                               event.position.x(),
                                               event.position.y(),
                                               0.f},
                                              physical_.modifiers});
    }
}
void GameInput::apply(std::vector<host::InputEvent>& output, input::WheelMotion event) {
    if (physical_.focus == input::WindowFocus::Lost) {
        return;
    }
    if (route_.context == Context::Menu) {
        output.emplace_back(host::MenuPointer{{host::PointerAction::Wheel,
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
        button(output, key, ButtonState::Pressed);
        button(output, key, ButtonState::Released);
    }
}
void GameInput::apply(std::vector<host::InputEvent>& output, input::PhysicalAxis event) {
    auto const axis{std::ranges::find(
        game_input_tables::gamepad_axes, event.key, &game_input_tables::GamepadAxisKeys::axis)};
    assert(axis != game_input_tables::gamepad_axes.end());
    auto const sample{event.position};
    physical_.gamepad_axes[axis - game_input_tables::gamepad_axes.begin()] = sample;
    auto const edge{axis_edge(physical_, axis->axis, std::abs(sample))};
    auto const focused{physical_.focus == input::WindowFocus::Gained};
    if (focused && route_.context == Context::Menu && menu_mode_ == input::MenuInputMode::Raw) {
        output.emplace_back(host::MenuAxis{event.key, sample, physical_.modifiers});
    } else if (focused && route_.context == Context::Gameplay) {
        if (edge) {
            flight_.button(state_, event.key, *edge, controls_);
        }
        flight_.sample_axis(state_, event.key, sample, controls_);
    }
    auto direction = [&](PhysicalKey key, float position) {
        if (key != PhysicalKey::Unbound) {
            if (auto change{axis_edge(physical_, key, position)}; change && focused) {
                button(output, key, *change);
            }
        }
    };
    if (sample >= 0.f) {
        direction(axis->negative, -sample);
        direction(axis->positive, sample);
    } else {
        direction(axis->positive, sample);
        direction(axis->negative, -sample);
    }
}
void GameInput::apply(std::vector<host::InputEvent>& output, input::GamepadDisconnected) {
    for (std::size_t i{}; i < game_input_tables::gamepad_axes.size(); ++i) {
        if (physical_.gamepad_axes[i] != 0.f) {
            apply(output, input::PhysicalAxis{game_input_tables::gamepad_axes[i].axis, 0.f});
        }
    }
    for (auto key : physical_.held) {
        if (physical_.focus == input::WindowFocus::Gained && is_gamepad_control(key.key)) {
            button(output, key, ButtonState::Released);
        }
    }
    std::erase_if(physical_.held, [](KeyCode key) { return is_gamepad_control(key.key); });
}
void GameInput::apply(std::vector<host::InputEvent>& output, input::WindowFocus focus) {
    if (std::exchange(physical_.focus, focus) == focus) {
        return;
    }
    if (focus == input::WindowFocus::Gained) {
        refresh_bindings(output);
        return;
    }

    release_context(output);
    flight_.reset(state_);
    physical_.modifiers = {};
    pointer_capture_ = {};
}
void GameInput::repeat_navigation(std::vector<host::InputEvent>& output, double seconds) {
    for (std::size_t i{}; i < game_input_tables::repeating_actions.size(); ++i) {
        auto& delay{repeat_delays_[i]};
        if (delay > 0.) {
            delay -= seconds;
            if (delay <= 0.) {
                auto const first{state_.transitions.size()};
                state_.button(game_input_tables::repeating_actions[i], ButtonState::Repeated);
                publish_transitions(output, first);
                delay = 0.1;
            }
        }
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
