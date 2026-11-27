#pragma once

#include <ioj/host/modifiers.h>
#include <ioj/input/actions.h>
#include <ioj/input/button_state.h>
#include <ioj/input/physical_key.h>
#include <ioj/vector.h>

#include <cstdint>
#include <string>
#include <variant>

namespace ioj {
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
