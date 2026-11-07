#pragma once

#include <ioj/levels/identifiers.h>
#include <ioj/levels/objective_name.h>
#include <ioj/sim/levels/level_entity_index.h>
#include <ioj/sim/missions/objective_id.h>

#include <unordered_map>

namespace ioj::sim {
struct SimClock;
class Timers;
struct LevelEventSchedule;

struct ExprCompilationContext {
    std::unordered_map<levels::EntityId, LevelEntityIndex> const& entities;
    std::unordered_map<levels::ObjectiveName, ObjectiveId> const& objectives;
    SimClock const& clock;
    Timers& timers;
    LevelEventSchedule& events;
};
}
