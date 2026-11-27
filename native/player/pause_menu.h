#pragma once

#include <ioj/host/input.h>
#include <ioj/host/pause_menu.h>
#include <ioj/input/actions.h>
#include <ioj/levels/launch_mode.h>
#include <ioj/sim/levels/level_state.h>

#include <optional>

namespace ioj {
struct InputRoute;

struct PauseMenu {
    using Item = host::PauseMenuItem;
    using Command = std::optional<host::PauseCommand>;

    auto handle(host::PlayerAction action, sim::LevelState simulation) -> Command;
    auto route(InputRoute underlying) const -> InputRoute;
    auto navigate(MenuCommand command, bool repeated) -> Command;
    auto activate(Item item) -> Command;
    auto widget_event(host::PauseWidgetEvent event) -> Command;
    auto simulation_request(sim::LevelState state) -> std::optional<levels::LaunchMode>;

    PauseMenuView view{PauseMenuView::Closed};
    Item selected{Item::Resume};
    Item page{Item::Audio};
    std::optional<Item> pending;
    bool options_modal{};
    bool controls_simulation{};
    bool pause_queued{};
};
}
