#include <ioj/session/launch_state.h>

#include <utility>

namespace ioj::session {
void LaunchState::set_pending_level(levels::LevelDefinition definition,
                                    std::string source_path,
                                    levels::LaunchOptions options) {
    pending_level_.emplace(PendingLevel{std::move(definition), std::move(source_path), options});
    launch_error_.clear();
}
auto LaunchState::take_pending_level() -> std::optional<PendingLevel> {
    return std::exchange(pending_level_, std::nullopt);
}
auto LaunchState::request_menu(MenuRequest request) -> bool {
    if (menu_request_) {
        return false;
    }
    pending_level_.reset();
    menu_request_ = std::move(request);
    return true;
}
auto LaunchState::take_menu_request() -> std::optional<MenuRequest> {
    return std::exchange(menu_request_, std::nullopt);
}
void LaunchState::set_launch_error(std::string error) {
    pending_level_.reset();
    launch_error_ = std::move(error);
}
auto LaunchState::take_launch_error() -> std::string {
    return std::exchange(launch_error_, {});
}
}
