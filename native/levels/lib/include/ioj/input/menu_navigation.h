#pragma once

#include <ioj/input/physical_key.h>

#include <cassert>
#include <optional>

namespace ioj::input {
enum class MenuCommand { Up, Down, Left, Right, Next, Previous, Accept, Back };
enum class MainMenuItem {
    SelectLevel,
    DataArchive,
    Video,
    Gameplay,
    Audio,
    Controls,
    Accessibility,
    System,
    Quit
};

constexpr auto menu_command(PhysicalKey key, bool shift) -> std::optional<MenuCommand> {
    switch (key) {
        case PhysicalKey::Up:
        case PhysicalKey::GamepadDPadUp:
        case PhysicalKey::GamepadLeftStickUp:
            return MenuCommand::Up;
        case PhysicalKey::Down:
        case PhysicalKey::GamepadDPadDown:
        case PhysicalKey::GamepadLeftStickDown:
            return MenuCommand::Down;
        case PhysicalKey::Left:
        case PhysicalKey::GamepadDPadLeft:
        case PhysicalKey::GamepadLeftStickLeft:
            return MenuCommand::Left;
        case PhysicalKey::Right:
        case PhysicalKey::GamepadDPadRight:
        case PhysicalKey::GamepadLeftStickRight:
            return MenuCommand::Right;
        case PhysicalKey::Tab:
            return shift ? MenuCommand::Previous : MenuCommand::Next;
        case PhysicalKey::Enter:
        case PhysicalKey::SpaceBar:
        case PhysicalKey::GamepadFaceButtonBottom:
            return MenuCommand::Accept;
        case PhysicalKey::Escape:
        case PhysicalKey::GamepadFaceButtonRight:
            return MenuCommand::Back;
        default:
            return {};
    }
}

constexpr auto move_main_selection(MainMenuItem selected, MenuCommand direction) -> MainMenuItem {
    assert(direction == MenuCommand::Up || direction == MenuCommand::Down);
    auto const count{static_cast<int>(MainMenuItem::Quit) + 1};
    auto const step{direction == MenuCommand::Up ? -1 : 1};
    return static_cast<MainMenuItem>((static_cast<int>(selected) + step + count) % count);
}
}
