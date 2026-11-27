#pragma once

#include <ioj/input/dispatch/input_actions.h>
#include <ioj/input/logical/actions.h>
#include <ioj/input/physical/modifiers.h>
#include <ioj/input/physical/physical_key.h>
#include <ioj/vector.h>

#include <optional>
#include <string>
#include <variant>

namespace ioj::host {
struct PointerInput {
    PointerAction action;
    PhysicalKey button;
    float x;
    float y;
    float wheel;
};

struct ViewerMotion {
    float right;
    float forward;
    float up;
    float yaw;
    float pitch;
    float speed;
    bool looking;
};

struct MenuButton {
    MenuCommand command;
    bool down;
    bool repeat;
    Modifiers modifiers;
};
struct MenuKey {
    PhysicalKey key;
    bool down{};
    bool repeat{};
    Modifiers modifiers{};
};
struct MenuAxis {
    PhysicalKey key;
    float value;
    Modifiers modifiers;
};
struct MenuPointer {
    PointerInput pointer;
    Modifiers modifiers;
};
struct MenuText {
    std::string text;
    Modifiers modifiers;
};
struct ViewerStep {
    ViewerMotion motion;
    double seconds;
};
using PresentationEvent =
    std::variant<MenuKey, MenuAxis, MenuPointer, MenuText>;
using MenuEvent = std::variant<PlayerAction, MenuButton>;
}
