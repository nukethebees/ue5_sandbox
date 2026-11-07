#pragma once

#include <ioj/levels/level_metadata.h>
#include <ioj/sim/expressions/expr_manager.h>
#include <ioj/sim/expressions/timers.h>
#include <ioj/sim/levels/compiled_level_events.h>
#include <ioj/sim/missions/mission.h>

namespace ioj::sim {
struct CompiledLevel {
    CompiledLevelEvents events;
    Mission mission;
    ExprManager expressions;
    Timers timers;
    levels::LevelMetadata level_metadata;
};
}
