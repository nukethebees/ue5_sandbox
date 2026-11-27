#pragma once

#include "player/engine.h"
#include "player/loaded_level.h"
#include <ioj/host/unreal_messages.h>
#include <ioj/levels/launch_options.h>
#include <ioj/levels/level_definition.h>
#include <ioj/profiles/profile_manager.h>
#include <ioj/profiles/save_browser.h>
#include <ioj/settings/player_settings.h>
#include <ioj/sim/levels/level_sim.h>

#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace ioj {
struct CliArgs;

class Application {
  public:
    static auto run(CliArgs args) -> ApplicationResult;

  private:
    struct AvailableLevel {
        levels::LevelDefinition definition;
        std::filesystem::path source_path;
    };

    explicit Application(profiles::ProfileManager profiles);

    auto play(Window& window,
              std::filesystem::path const& level_path,
              std::filesystem::path const& scripts_directory,
              bool exit_when_ready) -> ApplicationResult;
    auto publish_catalog(std::filesystem::path const& scripts_directory) const
        -> std::vector<AvailableLevel>;
    void publish_profiles() const;
    void process_profile_actions(std::span<host::ProfileAction const> actions);
    void process_preferences(Window& window, std::span<host::PreferenceEdit const> edits);
    auto launch_level(std::filesystem::path const& path) -> ApplicationResult;
    auto launch_level(levels::LevelDefinition const& definition,
                      std::string source_path,
                      levels::LaunchOptions options) -> ApplicationResult;
    auto initialize_presentation(LoadedLevel& level) -> bool;
    void advance_level(sim::LevelSim& simulation, double delta_seconds);
    void unload_level(sim::RunEndReason reason, bool show_results);

    PlayerSettings settings_;
    profiles::ProfileManager profiles_;
    profiles::SaveGameBrowser profile_browser_;
    LevelStorage levels_;
};
}
