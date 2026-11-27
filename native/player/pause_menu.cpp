#include "player/pause_menu.h"

#include "player/game_input.h"

namespace ioj {
auto PauseMenu::handle(host::PlayerAction action, sim::LevelState simulation) -> Command {
    if (action == host::PlayerAction::RequestResume) {
        return activate(Item::Resume);
    }
    if (action != host::PlayerAction::OpenPause || view != View::Closed ||
        (simulation != sim::LevelState::Running && simulation != sim::LevelState::Paused)) {
        return {};
    }
    view = View::Sidebar;
    selected = Item::Resume;
    page = Item::Audio;
    controls_simulation = true;
    return host::PauseCommand{host::PauseCommandKind::Open, selected};
}

auto PauseMenu::route(InputRoute underlying) const -> InputRoute {
    if (view != View::Closed) {
        return {InputContext::Menu, view != View::Leaving && !options_modal && !pending};
    }
    return underlying;
}

auto PauseMenu::activate(Item item) -> Command {
    if (view == View::Closed || view == View::Leaving || options_modal || pending || item == page) {
        return {};
    }
    pending = item;
    return host::PauseCommand{host::PauseCommandKind::CheckLeave, item};
}

auto PauseMenu::navigate(MenuCommand command, bool repeated) -> Command {
    using enum MenuCommand;
    if (options_modal || pending || view != View::Sidebar) {
        return {};
    }
    switch (command) {
        case Accept:
            return repeated ? Command{} : activate(selected);
        case Back:
            return repeated ? Command{} : activate(Item::Resume);
        case Right:
            view = View::Options;
            return host::PauseCommand{host::PauseCommandKind::ShowPage, page};
        case Left:
            return {};
        default:
            break;
    }
    auto const backwards{command == Up || command == Previous};
    do {
        selected = backwards ? cycle_previous(selected) : cycle_next(selected);
    } while (selected == page);
    return host::PauseCommand{host::PauseCommandKind::Select, selected};
}

auto PauseMenu::widget_event(host::PauseWidgetEvent event) -> Command {
    if (view == View::Closed || view == View::Leaving) {
        return {};
    }
    using enum host::PauseWidgetEventKind;
    switch (event.kind) {
        case Select:
            if (!options_modal && !pending && event.item != page &&
                (view != View::Sidebar || selected != event.item)) {
                view = View::Sidebar;
                selected = event.item;
                return host::PauseCommand{host::PauseCommandKind::Select, selected};
            }
            break;
        case Activate:
            return activate(event.item);
        case ContentFocused:
            view = View::Options;
            break;
        case ModalOpened:
        case ModalClosed:
            options_modal = event.kind == ModalOpened;
            break;
        case LeaveCancelled:
            if (pending == event.item) {
                pending.reset();
            }
            break;
        case LeaveAccepted:
            if (pending != event.item) {
                break;
            }
            pending.reset();
            selected = event.item;
            if (selected == Item::Controls || selected == Item::Audio) {
                page = selected;
                view = View::Options;
                return host::PauseCommand{host::PauseCommandKind::ShowPage, page};
            }
            view = selected == Item::Resume ? View::Closed : View::Leaving;
            return host::PauseCommand{host::PauseCommandKind::Finish, selected};
    }
    return {};
}

auto PauseMenu::simulation_request(sim::LevelState state) -> std::optional<levels::LaunchMode> {
    if (!controls_simulation) {
        return {};
    }
    if (state == sim::LevelState::Paused) {
        pause_queued = false;
        if (view == View::Closed) {
            controls_simulation = false;
            return levels::LaunchMode::Running;
        }
    } else if (state == sim::LevelState::Running && !pause_queued) {
        if (view == View::Closed) {
            controls_simulation = false;
        } else {
            pause_queued = true;
            return levels::LaunchMode::Paused;
        }
    }
    return {};
}
}
