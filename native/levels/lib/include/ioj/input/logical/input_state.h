#pragma once

#include <ioj/enum_array.h>
#include <ioj/input/logical/actions.h>
#include <ioj/input/physical/button_state.h>
#include <ioj/vector.h>

#include <optional>
#include <vector>

namespace ioj {
struct ActionTransition {
    DigitalAction action;
    ButtonState state;
};

struct InputState {
    void button(DigitalAction action, ButtonState state);
    void begin_frame();

    std::vector<ActionTransition> transitions;
    std::optional<Vector2d> pointer_steering;
    EnumArray<Vector2f, Axis2DAction> axes_2d;
    EnumArray<float, Axis1DAction> axes_1d;
    DigitalActions held;
    ObserverSpeedPreset observer_speed{ObserverSpeedPreset::Normal};
    ObserverLookMode observer_look{ObserverLookMode::Pointer};
};
}
