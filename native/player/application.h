#pragma once

#include "player/engine.h"
#include <ioj/host/unreal_messages.h>
#include <ioj/levels/authoring/definition_catalog.h>
#include <ioj/memory/bump_resource.h>
#include <ioj/profiles/profile_manager.h>
#include <ioj/profiles/save_browser.h>
#include <ioj/session/mission_launch_request.h>
#include <ioj/settings/player_settings.h>
#include <ioj/sim/levels/level_sim.h>

#include <filesystem>
#include <optional>
#include <span>

namespace ioj {
struct CliArgs;

class Application {
  public:
    static auto run(CliArgs args) -> ApplicationResult;

  private:
    explicit Application(profiles::ProfileManager profiles);

    auto play(Window& window, std::filesystem::path const& mission_path, bool exit_when_ready)
        -> ApplicationResult;
    void publish_catalog();
    void publish_profiles() const;
    void process_profile_actions(std::span<host::ProfileAction const> actions);
    void process_preferences(Window& window, std::span<host::PreferenceEdit const> edits);
    auto launch_mission(std::filesystem::path const& path) -> ApplicationResult;
    auto launch_mission(session::MissionLaunchRequest mission) -> ApplicationResult;
    void bind_level();
    void advance_level(double delta_seconds);
    void unload_level(sim::RunEndReason reason, bool show_results);

    PlayerSettings settings_;
    std::filesystem::path scripts_directory_;
    std::optional<levels::authoring::DefinitionCatalog> catalog_;
    std::optional<double> mission_duration_seconds_;
    std::optional<session::MissionLaunchRequest> mission_to_bind_;
    profiles::ProfileManager profiles_;
    profiles::SaveGameBrowser profile_browser_;
    memory::BumpResource level_arena_;
    std::optional<sim::LevelSim> level_;
};
}
