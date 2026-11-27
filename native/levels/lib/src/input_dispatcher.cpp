#include <ioj/input/dispatch/input_dispatcher.h>

#include <ioj/input/logical/input_state.h>

#include <utility>

namespace ioj {
void InputDispatcher::begin_frame() {
    order_.clear();
    actions_.clear();
    buttons_.clear();
    candidates_.clear();
    keys_.clear();
    pointers_.clear();
    text_.clear();
    ship_events_.clear();
}
void InputDispatcher::push(host::PlayerAction event) {
    order_.push_back({Kind::PlayerAction, actions_.size()});
    actions_.push_back(event);
}
void InputDispatcher::push(host::MenuButton event) {
    order_.push_back({Kind::MenuButton, buttons_.size()});
    buttons_.push_back(event);
}
void InputDispatcher::push(PhysicalKey event) {
    order_.push_back({Kind::PhysicalKey, candidates_.size()});
    candidates_.push_back(event);
}
void InputDispatcher::push(host::MenuKey event) {
    order_.push_back({Kind::MenuKey, keys_.size()});
    keys_.push_back(event);
}
void InputDispatcher::push(host::MenuPointer event) {
    order_.push_back({Kind::MenuPointer, pointers_.size()});
    pointers_.push_back(event);
}
void InputDispatcher::push(host::MenuText event) {
    order_.push_back({Kind::MenuText, text_.size()});
    text_.push_back(std::move(event));
}
void InputDispatcher::publish_ship(LogicalInputState const& state) {
    using enum sim::player::PlayerInputKind;
    ship_events_.clear();
    for (auto const [action, edge] : state.transitions) {
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
    auto const turn{state.axes_2d[Axis2DAction::FlightTurn]};
    pitch_ = turn.y();
    yaw_ = turn.x();
    accelerator_ = state.axes_1d[Axis1DAction::Accelerate];
}
auto InputDispatcher::ship_input() const -> sim::player::PlayerInputFrame {
    return {ship_events_, pitch_, yaw_, accelerator_};
}
}
