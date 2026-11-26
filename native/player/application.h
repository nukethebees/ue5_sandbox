#pragma once

#include "player/engine.h"
#include <ioj/system_capabilities.h>
#include <ioj/host/unreal_messages.h>
#include <ioj/memory/bump_resource.h>
#include <ioj/profiles/profile_manager.h>
#include <ioj/profiles/save_browser.h>
#include <ioj/session/pending_level.h>
#include <ioj/settings/player_settings.h>
#include <ioj/sim/levels/level_sim.h>

#include <expected>
#include <optional>
#include <span>
#include <string>

namespace ioj::player {
class Application {
  public:
    explicit Application(CliArgs args);
    // Run one engine instance through shutdown.
    auto run() -> std::expected<void, std::string>;
  private:
    auto play(Window& window) -> Result;
    void process_profile_actions(std::span<host::ProfileAction const> actions);
    void process_preferences(Window& window, std::span<host::PreferenceEdit const> edits);
    auto launch_mission(std::filesystem::path const& path) -> bool;
    void create_level(session::PendingLevel& pending);
    void advance_level(double delta_seconds);
    void unload_level(sim::RunEndReason reason, bool show_results);

    CliArgs const args_;
    LaunchPaths paths_;
    SystemCapabilities const capabilities_;
    PlayerSettings settings_;
    std::optional<session::PendingLevel> pending_level_;
    profiles::ProfileManager profiles_;
    profiles::SaveGameBrowser profile_browser_;
    memory::BumpResource level_arena_;
    std::optional<sim::LevelSim> level_;
};
}
