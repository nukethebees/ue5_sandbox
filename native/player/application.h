#pragma once

#include "player/engine.h"
#include "player/input/input_system.h"
#include "player/loaded_level.h"
#include "player/native_endpoint.h"
#include "player/window.h"
#include <ioj/levels/level_definition.h>
#include <ioj/profiles/profile_manager.h>
#include <ioj/profiles/save_browser.h>
#include <ioj/settings/player_settings.h>

#include <filesystem>
#include <vector>

namespace ioj {
class Application {
  public:
    Application(Window window,
                PlayerSettings settings,
                profiles::ProfileManager profiles,
                std::filesystem::path scripts_directory,
                bool exit_when_ready);
    Application(Application const&) = delete;
    Application(Application&&) = delete;
    auto operator=(Application const&) -> Application& = delete;
    auto operator=(Application&&) -> Application& = delete;
    auto run(std::wstring const& engine_command_line, std::filesystem::path const& level_path)
        -> ApplicationResult;
  private:
    struct AvailableLevel {
        levels::LevelDefinition definition;
        std::filesystem::path source_path;
    };
    auto play(std::filesystem::path const& level_path) -> ApplicationResult;
    void exchange();
    void process_input(double seconds);
    void consume_commands();
    auto route() const -> InputRoute;
    void handle_action(host::PlayerAction action);
    void choose_pause(host::PauseMenuItem item);
    void finish_leave(bool accepted);
    void apply_menu(MenuUpdate update);
    void present_menu(MenuUpdate update);
    void apply_preferences(bool keep_display);
    void revert_display();
    void finish_preferences(host::SettingsApplied result);
    void publish_settings();
    void publish_catalog();
    void publish_profiles();
    void profile_action(MenuControl action);
    void return_to_menu(std::string error = {});
    auto launch_level(std::filesystem::path const& path) -> ApplicationResult;
    auto launch_level(levels::LevelDefinition const& definition,
                      std::string source_path,
                      levels::LaunchOptions options) -> ApplicationResult;
    void unload_level(sim::RunEndReason reason, bool show_results);

    Window window_;
    LevelStorage storage_;
    PlayerSettings settings_;
    PlayerMenu menu_;
    profiles::ProfileManager profiles_;
    profiles::SaveGameBrowser profile_browser_;
    host::NativeEndpoint endpoint_;
    InputSystem input_;
    host::EngineStatus engine_{};
    std::filesystem::path scripts_directory_;
    std::vector<AvailableLevel> available_levels_;
    ApplicationResult result_{};
    bool quit_requested_{};
    bool const exit_when_ready_;
};
}
