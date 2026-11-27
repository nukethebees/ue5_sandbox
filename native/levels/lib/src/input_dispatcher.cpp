#include <ioj/input/dispatch/input_dispatcher.h>

#include <ioj/input/logical/input_state.h>

#include <utility>

namespace ioj {
void InputDispatcher::begin_frame() {
    order_.clear();
    menu_.clear();
    presentation_.clear();
    ship_events_.clear();
}
void InputDispatcher::push(host::MenuEvent event) {
    menu_.push_back(std::move(event));
    order_.push_back({false, menu_.size() - 1});
}
void InputDispatcher::push(host::PresentationEvent event) {
    presentation_.push_back(std::move(event));
    order_.push_back({true, presentation_.size() - 1});
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
