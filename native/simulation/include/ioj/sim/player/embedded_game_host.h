#pragma once

#include <ioj/settings/application_settings.h>
#include <ioj/sim/player/game_settings_session.h>

namespace ioj {
enum class MissionStartup { Pending, Ready, Failed };

// Owned by SandboxHost from before engine initialization through shutdown.
struct EmbeddedGameHost {
    ApplicationSettings application;
    GameSettingsSession session;
    wchar_t const* preferences_path;
    void (*apply_display)(GameSettingsState const&);
    MissionStartup mission_startup{MissionStartup::Pending};
};
}
