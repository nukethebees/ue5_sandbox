#include "player/input/input_system.h"

#include "player/input/physical/physical_input_batch.h"

#include <algorithm>
#include <array>
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
constexpr std::array pointer_keys{
    PhysicalKey::LeftMouseButton,
    PhysicalKey::RightMouseButton,
    PhysicalKey::MiddleMouseButton,
    PhysicalKey::ThumbMouseButton,
    PhysicalKey::ThumbMouseButton2,
};
constexpr EnumArray<float, ObserverSpeedPreset> observer_speeds{
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
        case PhysicalKey::NumPadEnter:
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

}

InputSystem::InputSystem(SDL_Window& window,
                         ControlsSettings const& controls,
                         host::NativeEndpoint& endpoint)
    : controls_{controls}
    , devices_{window}
    , physical_{devices_.state()}
    , endpoint_{endpoint} {
    flight_.reset(state_);
}
auto InputSystem::relative_mouse() const -> bool {
    return route_.context == InputContext::Gameplay || observer_.look == ObserverLookMode::Camera;
}
auto InputSystem::editing_text() const -> bool {
    return route_.context == InputContext::Menu && menu_mode_ == MenuInputMode::TextEntry;
}
auto InputSystem::poll() -> WindowInputEvents {
    state_.begin_frame();
    commands_.clear();
    return devices_.poll();
}
void InputSystem::release_context() {
    auto const first{state_.transitions.size()};
    for (auto const& button : routed_buttons_) {
        state_.button(button.action, ButtonState::Released);
    }
    publish_transitions(first);
    routed_buttons_.clear();
    repeat_delays_ = {};
    observer_.look = ObserverLookMode::Pointer;
    state_.axes_2d[Axis2DAction::ObserverLookDelta] = {};
}
void InputSystem::synchronize(InputRoute route, MenuInputMode mode) {
    if (mode != menu_mode_) {
        menu_mode_ = mode;
        repeat_delays_ = {};
        if (route_.context == InputContext::Menu) {
            release_context();
        }
    }
    if (route != route_) {
        release_context();
        route_ = route;
        refresh_bindings();
    }
    devices_.set_window_modes(relative_mouse(), editing_text());
}
void InputSystem::refresh_bindings() {
    auto const first{state_.transitions.size()};
    flight_.reset(state_);
    publish_transitions(first);
    if (route_.context == InputContext::Gameplay && physical_.focus == WindowFocus::Gained) {
        for (auto key : physical_.held) {
            flight_.axis(state_, key, 1.f, controls_);
        }
        for (std::size_t i{}; i < game_input_tables::gamepad_axes.size(); ++i) {
            flight_.axis(state_,
                         game_input_tables::gamepad_axes[i].axis,
                         physical_.gamepad_axes[i],
                         controls_);
        }
    }
}
void InputSystem::publish_transitions(std::size_t first) {
    for (auto const payload : std::span{state_.transitions}.subspan(first)) {
        if (auto command{menu_command(payload.action)}) {
            commands_.emplace_back(MenuButton{
                *command, payload.state != ButtonState::Released, false, physical_.modifiers});
            continue;
        }
        if (payload.state == ButtonState::Pressed) {
            using enum host::PlayerAction;
            switch (payload.action) {
                case DigitalAction::Pause:
                    commands_.emplace_back(route_.resume_available ? RequestResume : OpenPause);
                    break;
                case DigitalAction::BenchmarkNext:
                    commands_.emplace_back(BenchmarkNext);
                    break;
                case DigitalAction::BenchmarkActivate:
                    commands_.emplace_back(BenchmarkActivate);
                    break;
                case DigitalAction::BenchmarkToggle:
                    commands_.emplace_back(ToggleBenchmark);
                    break;
                case DigitalAction::BenchmarkEnd:
                    commands_.emplace_back(EndBenchmark);
                    break;
                default:
                    break;
            }
        }
    }
}
void InputSystem::routed_button(PhysicalKey key, DigitalAction action, ButtonState edge) {
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
        routed_buttons_.erase(found);
    }

    auto const direction{std::ranges::find(digital_action_repeatable_navigation_values, action)};
    if (direction != digital_action_repeatable_navigation_values.end() && is_gamepad_control(key)) {
        auto& delay{
            repeat_delays_[direction - digital_action_repeatable_navigation_values.begin()]};
        if (edge == ButtonState::Pressed && delay == 0.) {
            delay = 0.4;
        } else if (edge == ButtonState::Released &&
                   !std::ranges::any_of(routed_buttons_, [action](auto source) {
                       return source.action == action && is_gamepad_control(source.key);
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
    publish_transitions(first);
    if (action == DigitalAction::ObserverToggleLook && first != state_.transitions.size() &&
        edge == ButtonState::Pressed) {
        observer_.look = observer_.look == ObserverLookMode::Pointer ? ObserverLookMode::Camera
                                                                     : ObserverLookMode::Pointer;
    }
}
void InputSystem::button(PhysicalKey key, ButtonState edge, bool repeat) {
    auto const down{edge != ButtonState::Released};
    if (route_.context == InputContext::Menu && menu_mode_ == MenuInputMode::BindingCapture) {
        if (down && !repeat) {
            commands_.emplace_back(key);
        }
        return;
    }
    if ((route_.resume_available || route_.context == InputContext::Observer ||
         route_.context == InputContext::Gameplay) &&
        (route_.context == InputContext::Gameplay || is_pause_button(controls_.bindings, key))) {
        auto const first{state_.transitions.size()};
        if (!repeat) {
            flight_.button(state_, key, edge, controls_, route_.context);
        }
        publish_transitions(first);
        return;
    }
    if (editing_text() && key != PhysicalKey::Tab && key != PhysicalKey::Escape &&
        key != PhysicalKey::Enter && key != PhysicalKey::NumPadEnter && !is_gamepad_control(key)) {
        endpoint_.send_key(host::MenuKey{key, down, repeat, physical_.modifiers});
        return;
    }
    if (repeat || edge == ButtonState::Released) {
        auto const source{std::ranges::find(routed_buttons_, key, &RoutedButton::key)};
        if (source != routed_buttons_.end() && (!repeat || route_.context == InputContext::Menu)) {
            if (repeat) {
                if (auto command{menu_command(source->action)}) {
                    commands_.emplace_back(MenuButton{*command, true, true, physical_.modifiers});
                }
            } else {
                routed_button(key, source->action, edge);
            }
        }
        return;
    }
    auto const navigation{
        menu_action(key,
                    route_.context == InputContext::Menu &&
                        (physical_.modifiers.left_shift() || physical_.modifiers.right_shift()))};
    if (route_.context == InputContext::Menu && navigation) {
        routed_button(key, *navigation, edge);
        return;
    }
    if (route_.context == InputContext::Observer) {
        if (auto action{observer_action(key)}) {
            routed_button(key, *action, edge);
        }
        return;
    }
    if (route_.context == InputContext::Benchmark) {
        if (navigation) {
            auto const action{
                *navigation == DigitalAction::MenuAccept ? DigitalAction::BenchmarkActivate
                : *navigation == DigitalAction::MenuBack ? DigitalAction::BenchmarkEnd
                                                         : DigitalAction::BenchmarkNext};
            routed_button(key, action, edge);
        } else if (is_pause_button(controls_.bindings, key)) {
            routed_button(key, DigitalAction::BenchmarkToggle, edge);
        }
    }
}
auto InputSystem::map_events(PhysicalInputBatch const& events, std::size_t first) -> std::size_t {
    auto const count{events.order.size()};
    auto const buttons{events.buttons.view()};
    auto const pointer_buttons{events.pointer_buttons.view()};
    auto const axes{events.axes.view()};
    auto const motions{events.motions.view()};
    auto const wheels{events.wheels.view()};
    while (first < count) {
        auto const initial_commands{commands_.size()};
        auto const initial_messages{endpoint_.size()};
        auto const [kind, row]{events.order[first++]};
        switch (kind) {
            case PhysicalEventKind::Button:
                apply_button(buttons.keys()[row],
                             buttons.modifiers()[row],
                             buttons.states()[row],
                             buttons.repeats()[row]);
                break;
            case PhysicalEventKind::PointerButton:
                apply_pointer_button(pointer_buttons.positions()[row],
                                     pointer_buttons.keys()[row],
                                     pointer_buttons.states()[row],
                                     pointer_buttons.clicks()[row]);
                break;
            case PhysicalEventKind::Axis:
                apply_axis(axes.keys()[row], axes.positions()[row]);
                break;
            case PhysicalEventKind::PointerMotion:
                apply_motion(motions.positions()[row], motions.deltas()[row]);
                break;
            case PhysicalEventKind::Wheel:
                apply_wheel(wheels.positions()[row], wheels.steps()[row]);
                break;
            case PhysicalEventKind::Text:
                apply_text(events.text[row]);
                break;
            case PhysicalEventKind::FocusGained:
                apply_focus(WindowFocus::Gained);
                break;
            case PhysicalEventKind::FocusLost:
                apply_focus(WindowFocus::Lost);
                break;
            case PhysicalEventKind::Disconnect:
                apply_disconnect();
                break;
        }
        auto const commands{std::span{commands_}.subspan(initial_commands)};
        auto const routing_change{std::ranges::any_of(commands, [](InputCommand const& command) {
            switch (command.kind) {
                case InputCommand::Kind::Capture:
                    return true;
                case InputCommand::Kind::Navigation:
                    return command.button.down;
                case InputCommand::Kind::Action:
                    return command.action != host::PlayerAction::BenchmarkNext &&
                           command.action != host::PlayerAction::ToggleBenchmark;
            }
            std::unreachable();
        })};
        auto const same_slate_stream{
            first < count && events.order[first].kind == kind &&
            (kind == PhysicalEventKind::Text || kind == PhysicalEventKind::PointerMotion)};
        if (routing_change || (endpoint_.size() != initial_messages && !same_slate_stream)) {
            break;
        }
    }
    return first;
}
void InputSystem::apply_button(PhysicalKey key,
                               host::Modifiers modifiers,
                               ButtonState state,
                               bool repeat) {
    if (!is_gamepad_control(key)) {
        physical_.modifiers = modifiers;
    }
    if (!physical_.track_button(key, state, repeat) || physical_.focus == WindowFocus::Lost) {
        return;
    }
    button(key, state, repeat);
    auto const active_modifiers{physical_.modifiers};
    if (editing_text() && key == PhysicalKey::BackSpace && state != ButtonState::Released &&
        !active_modifiers.left_control() && !active_modifiers.right_control() &&
        !active_modifiers.left_alt() && !active_modifiers.right_alt()) {
        endpoint_.send_text(host::MenuText{"\b", active_modifiers});
    }
}
void InputSystem::apply_text(std::string const& text) {
    if (physical_.focus == WindowFocus::Gained && editing_text()) {
        endpoint_.send_text(host::MenuText{text, physical_.modifiers});
    }
}
void InputSystem::apply_pointer_button(Vector2f position,
                                       PhysicalKey key,
                                       ButtonState state,
                                       std::uint8_t clicks) {
    if (!physical_.track_button(key, state) || physical_.focus == WindowFocus::Lost) {
        return;
    }
    auto const found{std::ranges::find(game_input_tables::pointer_keys, key)};
    assert(found != game_input_tables::pointer_keys.end());
    auto const mask{
        static_cast<std::uint8_t>(1u << (found - game_input_tables::pointer_keys.begin()))};
    auto const captured{(pointer_capture_ & mask) != 0};
    if (route_.context == InputContext::Menu && menu_mode_ == MenuInputMode::BindingCapture &&
        !captured) {
        if (state == ButtonState::Pressed) {
            commands_.emplace_back(key);
        }
        return;
    }
    if ((route_.resume_available || route_.context == InputContext::Observer) &&
        is_pause_button(controls_.bindings, key) && !captured) {
        button(key, state);
        return;
    }
    auto const down{state == ButtonState::Pressed};
    auto const visible{!relative_mouse()};
    auto const camera_toggle{route_.context == InputContext::Observer &&
                             key == PhysicalKey::RightMouseButton};
    if ((down && visible && !camera_toggle) || (!down && captured)) {
        auto action{host::PointerAction::Up};
        if (down) {
            pointer_capture_ |= mask;
            action = clicks == 2 ? host::PointerAction::DoubleClick : host::PointerAction::Down;
        } else {
            pointer_capture_ &= ~mask;
        }
        endpoint_.send_pointer(
            host::MenuPointer{action, key, position.x(), position.y(), 0.f, physical_.modifiers});
    }
    if (route_.context != InputContext::Menu &&
        !(route_.context == InputContext::Benchmark && key == PhysicalKey::LeftMouseButton)) {
        button(key, state);
    }
}
void InputSystem::apply_motion(Vector2f position, Vector2f delta) {
    if (physical_.focus == WindowFocus::Lost) {
        return;
    }
    if (route_.context == InputContext::Gameplay) {
        flight_.pointer(state_, delta, controls_);
    } else if (observer_.look == ObserverLookMode::Camera) {
        auto& look{state_.axes_2d[Axis2DAction::ObserverLookDelta]};
        look.x() += delta.x();
        look.y() -= delta.y();
    } else if (menu_mode_ != MenuInputMode::BindingCapture) {
        endpoint_.send_pointer(host::MenuPointer{host::PointerAction::Move,
                                                 PhysicalKey::Unbound,
                                                 position.x(),
                                                 position.y(),
                                                 0.f,
                                                 physical_.modifiers});
    }
}
void InputSystem::apply_wheel(Vector2f position, float steps) {
    if (physical_.focus == WindowFocus::Lost) {
        return;
    }
    if (route_.context == InputContext::Menu) {
        if (menu_mode_ == MenuInputMode::BindingCapture) {
            if (steps != 0.f) {
                commands_.emplace_back(steps > 0.f ? PhysicalKey::MouseScrollUp
                                                   : PhysicalKey::MouseScrollDown);
            }
            return;
        }
        endpoint_.send_pointer(host::MenuPointer{host::PointerAction::Wheel,
                                                 PhysicalKey::Unbound,
                                                 position.x(),
                                                 position.y(),
                                                 steps,
                                                 physical_.modifiers});
    } else if (route_.context == InputContext::Observer) {
        auto const step{(steps > 0.f) - (steps < 0.f)};
        observer_.speed = static_cast<ObserverSpeedPreset>(
            std::clamp<int>(std::to_underlying(observer_.speed) + step,
                            std::to_underlying(enum_min<ObserverSpeedPreset>),
                            std::to_underlying(enum_max<ObserverSpeedPreset>)));
    } else if (route_.context == InputContext::Gameplay && steps != 0.f) {
        auto const key{steps > 0.f ? PhysicalKey::MouseScrollUp : PhysicalKey::MouseScrollDown};
        button(key, ButtonState::Pressed);
        button(key, ButtonState::Released);
    }
}
void InputSystem::apply_axis(PhysicalKey key, float position) {
    auto const axis{std::ranges::find(
        game_input_tables::gamepad_axes, key, &game_input_tables::GamepadAxisKeys::axis)};
    assert(axis != game_input_tables::gamepad_axes.end());
    auto const sample{position};
    physical_.gamepad_axes[axis - game_input_tables::gamepad_axes.begin()] = sample;
    auto const edge{physical_.axis_edge(axis->axis, std::abs(sample))};
    auto const focused{physical_.focus == WindowFocus::Gained};
    if (focused && route_.context == InputContext::Menu &&
        menu_mode_ == MenuInputMode::BindingCapture) {
        if (edge == ButtonState::Pressed) {
            commands_.emplace_back(key);
        }
    } else if (focused && route_.context == InputContext::Gameplay) {
        if (edge) {
            flight_.button(state_, key, *edge, controls_, route_.context);
        }
        flight_.axis(state_, key, sample, controls_);
    }
    auto direction = [&](PhysicalKey key, float position) {
        if (key != PhysicalKey::Unbound) {
            if (auto change{physical_.axis_edge(key, position)};
                change && focused &&
                !(route_.context == InputContext::Menu &&
                  menu_mode_ == MenuInputMode::BindingCapture)) {
                button(key, *change);
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
void InputSystem::apply_disconnect() {
    for (std::size_t i{}; i < game_input_tables::gamepad_axes.size(); ++i) {
        if (physical_.gamepad_axes[i] != 0.f) {
            apply_axis(game_input_tables::gamepad_axes[i].axis, 0.f);
        }
    }
    for (auto key : physical_.held) {
        if (physical_.focus == WindowFocus::Gained && is_gamepad_control(key)) {
            button(key, ButtonState::Released);
        }
    }
    std::erase_if(physical_.held, [](PhysicalKey key) { return is_gamepad_control(key); });
}
void InputSystem::apply_focus(WindowFocus focus) {
    if (std::exchange(physical_.focus, focus) == focus) {
        return;
    }
    if (focus == WindowFocus::Gained) {
        refresh_bindings();
        return;
    }

    release_context();
    flight_.reset(state_);
    physical_.modifiers = {};
    pointer_capture_ = {};
}
void InputSystem::repeat_navigation(double seconds) {
    if (!devices_.focused()) {
        return;
    }
    for (std::size_t i{}; i < digital_action_repeatable_navigation_values.size(); ++i) {
        auto& delay{repeat_delays_[i]};
        if (delay > 0.) {
            delay -= seconds;
            if (delay <= 0.) {
                auto const command{menu_command(digital_action_repeatable_navigation_values[i])};
                assert(command);
                commands_.emplace_back(MenuButton{*command, true, true, physical_.modifiers});
                delay = 0.1;
            }
        }
    }
}
auto InputSystem::viewer_step(double seconds) const -> std::optional<host::ViewerStep> {
    if (route_.context == InputContext::Observer) {
        auto const& held{state_.held};
        auto const look{state_.axes_2d[Axis2DAction::ObserverLookDelta]};
        auto const speed{game_input_tables::observer_speeds[observer_.speed] *
                         (held.observer_fast() ? 4.f : 1.f)};
        return host::ViewerStep{{float(held.observer_right() - held.observer_left()),
                                 float(held.observer_forward() - held.observer_backward()),
                                 float(held.observer_up() - held.observer_down()),
                                 look.x() * 0.1f,
                                 look.y() * 0.1f,
                                 speed,
                                 observer_.look == ObserverLookMode::Camera},
                                seconds};
    }
    return {};
}
auto InputSystem::ship_input() -> sim::player::PlayerInputFrame {
    using enum sim::player::PlayerInputKind;
    ship_events_.clear();
    for (auto const [action, edge] : state_.transitions) {
        auto const down{edge == ButtonState::Pressed};
        switch (action) {
            case DigitalAction::Fire:
                ship_events_.push_back(down ? FirePressed : FireReleased);
                break;
            case DigitalAction::Brake:
                ship_events_.push_back(down ? BrakePressed : BrakeReleased);
                break;
            case DigitalAction::EmergencyBrake:
                ship_events_.push_back(down ? EmergencyBrakePressed : EmergencyBrakeReleased);
                break;
            case DigitalAction::Boost:
                ship_events_.push_back(down ? BoostPressed : BoostReleased);
                break;
            default:
                break;
        }
    }
    auto const turn{state_.axes_2d[Axis2DAction::FlightTurn]};
    return {ship_events_, turn.y(), turn.x(), state_.axes_1d[Axis1DAction::Accelerate]};
}
}
