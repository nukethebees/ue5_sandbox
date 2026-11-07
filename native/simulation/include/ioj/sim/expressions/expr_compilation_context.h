#pragma once

#include <ioj/levels/identifiers.h>
#include <ioj/levels/objective_name.h>
#include <ioj/sim/expressions/timer_handle.h>
#include <ioj/sim/levels/level_entity_ref.h>
#include <ioj/sim/missions/objective_id.h>

#include <span>
#include <unordered_map>

namespace ioj::sim {
struct ExprCompilationContext {
    std::unordered_map<levels::EntityId, LevelEntityRef> const& entities;
    std::unordered_map<levels::ObjectiveName, ObjectiveId> const& objectives;
    std::span<TimerHandle const> timers;
};
}
