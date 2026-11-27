#pragma once

#include <ioj/host/input_actions.h>

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
}
