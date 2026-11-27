#pragma once

#include "player/engine.h"
#include "player/loaded_level.h"
#include <ioj/host/unreal_messages.h>
#include <ioj/levels/authoring/definition_catalog.h>
#include <ioj/levels/launch_options.h>
#include <ioj/profiles/profile_manager.h>
#include <ioj/profiles/save_browser.h>
#include <ioj/settings/player_settings.h>
#include <ioj/sim/levels/level_sim.h>

#include <filesystem>
#include <optional>
#include <span>
#include <string>

namespace ioj {
struct CliArgs;

class Application {
  public:
    static auto run(CliArgs args) -> ApplicationResult;

  private:
    explicit Application(profiles::ProfileManager profiles);

    auto play(Window& window, std::filesystem::path const& level_path, bool exit_when_ready)
        -> ApplicationResult;
    void publish_catalog();
    void publish_profiles() const;
    void process_profile_actions(std::span<host::ProfileAction const> actions);
    void process_preferences(Window& window, std::span<host::PreferenceEdit const> edits);
    auto launch_level(std::filesystem::path const& path) -> ApplicationResult;
    auto launch_level(levels::LevelDefinition const& definition,
                      std::string source_path,
                      levels::LaunchOptions options) -> ApplicationResult;
    void initialize_presentation(LoadedLevel& level);
    void advance_level(sim::LevelSim& simulation, double delta_seconds);
    void unload_level(sim::RunEndReason reason, bool show_results);

    PlayerSettings settings_;
    std::filesystem::path scripts_directory_;
    std::optional<levels::authoring::DefinitionCatalog> catalog_;
    profiles::ProfileManager profiles_;
    profiles::SaveGameBrowser profile_browser_;
    LevelStorage levels_;
};
}
