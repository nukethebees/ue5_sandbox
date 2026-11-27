#pragma once

#include <cstdint>

namespace ioj::host {
enum class PauseMenuItem : std::uint8_t { Resume, Audio, Controls, LevelSelect, Quit };
enum class PauseCommandKind : std::uint8_t { Open, Select, CheckLeave, ShowPage, Finish };
struct PauseCommand {
    PauseCommandKind kind;
    PauseMenuItem item{PauseMenuItem::Resume};
    bool operator==(PauseCommand const&) const = default;
};

enum class PauseWidgetEventKind : std::uint8_t {
    Select,
    Activate,
    ContentFocused,
    ModalOpened,
    ModalClosed,
    LeaveAccepted,
    LeaveCancelled
};
struct PauseWidgetEvent {
    PauseWidgetEventKind kind;
    PauseMenuItem item{PauseMenuItem::Resume};
};
}
