#pragma once

#include <ioj/levels/level_metadata.h>
#include <ioj/sim/levels/compiled_level_events.h>
#include <ioj/sim/missions/compiled_mission.h>

namespace ioj::sim {
struct CompiledLevel {
    CompiledLevelEvents events;
    CompiledMission mission;
    levels::LevelMetadata level_metadata;
};
}
