#include <ioj/levels/authoring/launch_state.h>

namespace ioj::levels::authoring {
void LaunchState::set_pending_level(LevelSource source,
                                    std::string source_path,
                                    LaunchOptions options) {
    pending_level_.emplace(
        PendingLevel{std::move(source), std::move(source_path), std::move(options)});
    launch_error_.clear();
}
auto LaunchState::take_pending_level() -> std::optional<PendingLevel> {
    return std::exchange(pending_level_, std::nullopt);
}
auto LaunchState::request_menu(MenuRequest request) -> bool {
    if (transitioning_) {
        return false;
    }
    pending_level_.reset();
    menu_request_ = std::move(request);
    transitioning_ = true;
    return true;
}
auto LaunchState::take_menu_request() -> std::optional<MenuRequest> {
    transitioning_ = false;
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
