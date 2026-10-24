#pragma once

#include <ioj/sim/levels/compiled_level_events.h>
#include <ioj/sim/levels/level_result_metadata.h>
#include <ioj/sim/missions/compiled_mission.h>

namespace ioj::sim {
struct CompiledLevel {
    CompiledLevelEvents events;
    CompiledMission mission;
    LevelResultMetadata result_metadata;
};
}
