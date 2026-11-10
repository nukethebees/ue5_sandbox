#pragma once

#include <ioj/session/level_select_request.h>
#include <ioj/session/pending_level.h>

#include <optional>
#include <string>

namespace ioj::session {
class LaunchState {
  public:
    void set_pending_level(levels::LevelDefinition definition,
                           std::string source_path,
                           levels::LaunchOptions options = {});
    [[nodiscard]] auto take_pending_level() -> std::optional<PendingLevel>;
    [[nodiscard]] auto request_menu(LevelSelectRequest request) -> bool;
    [[nodiscard]] auto take_menu_request() -> std::optional<LevelSelectRequest>;
    void set_launch_error(std::string error);
    [[nodiscard]] auto has_launch_error() const noexcept -> bool { return !launch_error_.empty(); }
    [[nodiscard]] auto take_launch_error() -> std::string;
  private:
    std::optional<PendingLevel> pending_level_;
    std::optional<LevelSelectRequest> menu_request_;
    std::string launch_error_;
};
}
