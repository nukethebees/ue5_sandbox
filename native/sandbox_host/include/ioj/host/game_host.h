#pragma once

#include <ioj/settings/launch_display.h>

namespace ioj {
enum class DisplayOperation { Inspect, Preview, Verify, Revert };
enum class MissionStartup { Pending, Ready, Failed };

// Return a borrowed diagnostic valid until the next display operation, or null on success.
using ApplyDisplay = auto(void*, DisplayOperation, HostDisplay&) -> char const*;

// Owned by SandboxHost from before engine initialization through shutdown.
struct GameHost {
    void* window{};
    ApplyDisplay* apply_display{};
    LaunchDisplay launch_display;
    MissionStartup mission_startup{MissionStartup::Pending};
};
}
