#pragma once

#include <cassert>

namespace ioj {
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

constexpr auto move_main_selection(MainMenuItem selected, MenuCommand direction) -> MainMenuItem {
    assert(direction == MenuCommand::Up || direction == MenuCommand::Down);
    auto const count{static_cast<int>(MainMenuItem::Quit) + 1};
    auto const step{direction == MenuCommand::Up ? -1 : 1};
    return static_cast<MainMenuItem>((static_cast<int>(selected) + step + count) % count);
}
}
