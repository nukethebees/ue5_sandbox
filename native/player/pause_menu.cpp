#include "player/pause_menu.h"

#include "player/input/logical/logical_input_system.h"

namespace ioj {
auto PauseMenu::handle(host::PlayerAction action, sim::LevelState simulation) -> Command {
    if (action == host::PlayerAction::RequestResume) {
        return activate(Item::Resume);
    }
    if (action != host::PlayerAction::OpenPause || view != PauseMenuView::Closed ||
        (simulation != sim::LevelState::Running && simulation != sim::LevelState::Paused)) {
        return {};
    }
    view = PauseMenuView::Sidebar;
    selected = Item::Resume;
    page = Item::Audio;
    controls_simulation = true;
    return host::PauseCommand{host::PauseCommandKind::Open, selected};
}

auto PauseMenu::route(InputRoute underlying) const -> InputRoute {
    if (view != PauseMenuView::Closed) {
        return {InputContext::Menu, view != PauseMenuView::Leaving && !options_modal && !pending};
    }
    return underlying;
}

auto PauseMenu::activate(Item item) -> Command {
    if (view == PauseMenuView::Closed || view == PauseMenuView::Leaving || options_modal ||
        pending || item == page) {
        return {};
    }
    pending = item;
    return host::PauseCommand{host::PauseCommandKind::CheckLeave, item};
}

auto PauseMenu::navigate(MenuCommand command, bool repeated) -> Command {
    using enum MenuCommand;
    if (options_modal || pending || view != PauseMenuView::Sidebar) {
        return {};
    }
    switch (command) {
        case Accept:
            return repeated ? Command{} : activate(selected);
        case Back:
            return repeated ? Command{} : activate(Item::Resume);
        case Right:
            view = PauseMenuView::Options;
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
    if (view == PauseMenuView::Closed || view == PauseMenuView::Leaving) {
        return {};
    }
    using enum host::PauseWidgetEventKind;
    switch (event.kind) {
        case Select:
            if (!options_modal && !pending && event.item != page &&
                (view != PauseMenuView::Sidebar || selected != event.item)) {
                view = PauseMenuView::Sidebar;
                selected = event.item;
                return host::PauseCommand{host::PauseCommandKind::Select, selected};
            }
            break;
        case Activate:
            return activate(event.item);
        case ContentFocused:
            view = PauseMenuView::Options;
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
                view = PauseMenuView::Options;
                return host::PauseCommand{host::PauseCommandKind::ShowPage, page};
            }
            view = selected == Item::Resume ? PauseMenuView::Closed : PauseMenuView::Leaving;
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
        if (view == PauseMenuView::Closed) {
            controls_simulation = false;
            return levels::LaunchMode::Running;
        }
    } else if (state == sim::LevelState::Running && !pause_queued) {
        if (view == PauseMenuView::Closed) {
            controls_simulation = false;
        } else {
            pause_queued = true;
            return levels::LaunchMode::Paused;
        }
    }
    return {};
}
}
