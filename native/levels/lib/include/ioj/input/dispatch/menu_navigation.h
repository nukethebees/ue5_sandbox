#pragma once

#include <ioj/input/logical/actions.h>

namespace ioj {
constexpr auto move_main_selection(MainMenuItem selected, MenuCommand direction) -> MainMenuItem {
    switch (direction) {
        case MenuCommand::Up:
            return cycle_previous(selected);
        case MenuCommand::Down:
            return cycle_next(selected);
        default:
            return selected;
    }
}
}
