#include <ioj/input/ship_input.h>

#include <ioj/input/input_state.h>

namespace ioj {
auto ship_input_frame(InputState const& state, std::vector<sim::player::PlayerInputKind>& events)
    -> sim::player::PlayerInputFrame {
    using enum sim::player::PlayerInputKind;
    events.clear();
    for (auto const [action, edge] : state.transitions) {
        if (edge == ButtonState::Repeated) {
            continue;
        }
        auto const down{edge == ButtonState::Pressed};
        switch (action) {
            case DigitalAction::Fire:
                events.push_back(down ? FirePressed : FireReleased);
                break;
            case DigitalAction::Brake:
                events.push_back(down ? BrakePressed : BrakeReleased);
                break;
            case DigitalAction::EmergencyBrake:
                events.push_back(down ? EmergencyBrakePressed : EmergencyBrakeReleased);
                break;
            case DigitalAction::Boost:
                events.push_back(down ? BoostPressed : BoostReleased);
                break;
            default:
                break;
        }
    }
    auto const turn{state.axes_2d[Axis2DAction::FlightTurn]};
    return {events, turn.y(), turn.x(), state.axes_1d[Axis1DAction::Accelerate]};
}
}
