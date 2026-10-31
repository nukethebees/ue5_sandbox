#pragma once

#include <ioj/levels/authoring/level_source.h>
#include <ioj/levels/launch_options.h>

#include <optional>
#include <string>
#include <utility>

namespace ioj::levels::authoring {
struct PendingLevel {
    LevelSource source;
    std::string source_path{};
    LaunchOptions options{};
};

enum class MenuDestination : std::uint8_t { LevelSelect, Telemetry };

struct MenuRequest {
    LevelId preferred_level_id{};
    MenuDestination destination{MenuDestination::LevelSelect};
    std::string selected_telemetry_run_id{};
    std::string telemetry_error{};
};

class LaunchState {
  public:
    void set_pending_level(LevelSource source, std::string source_path, LaunchOptions options = {});
    [[nodiscard]] auto take_pending_level() -> std::optional<PendingLevel>;
    [[nodiscard]] auto request_menu(MenuRequest request) -> bool;
    [[nodiscard]] auto take_menu_request() -> std::optional<MenuRequest>;
    void set_launch_error(std::string error);
    [[nodiscard]] auto has_launch_error() const noexcept -> bool { return !launch_error_.empty(); }
    [[nodiscard]] auto take_launch_error() -> std::string;
  private:
    std::optional<PendingLevel> pending_level_;
    std::optional<MenuRequest> menu_request_;
    std::string launch_error_;
    bool transitioning_{};
};
}
