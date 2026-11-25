#pragma once

#include "engine.h"
#include <ioj/game_capabilities.h>
#include <ioj/host/unreal_messages.h>
#include <ioj/profiles/profile_manager.h>
#include <ioj/profiles/save_browser.h>
#include <ioj/settings/player_settings.h>

#include <expected>
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
    CliArgs const args_;
    LaunchPaths paths_;
    GameCapabilities const capabilities_;
    PlayerSettings settings_;
    profiles::ProfileManager profiles_;
    profiles::SaveGameBrowser profile_browser_;
};
}
