#pragma once

#include <ioj/input/input_actions.h>
#include <ioj/input/logical/actions.h>
#include <ioj/input/physical/modifiers.h>
#include <ioj/input/physical/physical_key.h>
#include <ioj/vector.h>

#include <optional>
#include <string>

namespace ioj::host {
struct PauseCommand {
    PauseCommandKind kind;
    PauseMenuItem item{PauseMenuItem::Resume};
    bool operator==(PauseCommand const&) const = default;
};

struct PauseWidgetEvent {
    PauseWidgetEventKind kind;
    PauseMenuItem item{PauseMenuItem::Resume};
};

struct MenuPointer {
    PointerAction action;
    PhysicalKey button;
    float x;
    float y;
    float wheel;
    Modifiers modifiers;
};

struct ViewerMotion {
    float right;
    float forward;
    float up;
    float yaw;
    float pitch;
    float speed;
};

struct MenuKey {
    PhysicalKey key;
    bool down{};
    bool repeat{};
    Modifiers modifiers{};
};
struct MenuText {
    std::string text;
    Modifiers modifiers;
};
struct ViewerStep {
    ViewerMotion motion;
    double seconds;
};

}
