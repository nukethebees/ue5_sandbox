#pragma once

#include <ioj/input/physical/button_state.h>
#include <ioj/input/physical/modifiers.h>
#include <ioj/input/physical/physical_key.h>
#include <ioj/vector.h>

#include <array>
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace ioj {
struct PhysicalInputState {
    std::vector<PhysicalKey> held;
    std::array<float, 6> gamepad_axes{};
    host::Modifiers modifiers{};
    WindowFocus focus{WindowFocus::Gained};
};

struct PhysicalButton {
    PhysicalKey key;
    host::Modifiers modifiers{};
    ButtonState state{};
    bool repeat{};
};
struct PhysicalPointerButton {
    Vector2f position;
    PhysicalKey key;
    ButtonState state;
    std::uint8_t clicks;
};
struct PointerMotion {
    Vector2f position;
    Vector2f delta;
};
struct WheelMotion {
    Vector2f position;
    float steps;
};
struct PhysicalAxis {
    PhysicalKey key;
    float position;
};
struct TextInput {
    std::string text;
};
struct GamepadDisconnected {};
using PhysicalEvent = std::variant<PhysicalButton,
                                   PhysicalPointerButton,
                                   PhysicalAxis,
                                   PointerMotion,
                                   WheelMotion,
                                   TextInput,
                                   GamepadDisconnected,
                                   WindowFocus>;
}
