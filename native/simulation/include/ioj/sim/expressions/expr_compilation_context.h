#pragma once
#include <ioj/levels/identifiers.h>
#include <ioj/sim/expressions/timer_handle.h>
#include <ioj/sim/levels/level_entity_ref.h>

#include <span>
#include <unordered_map>
namespace ioj::sim {
struct ExprCompilationContext {
    std::unordered_map<levels::EntityId, LevelEntityRef> const& entities;
    std::span<TimerHandle const> timers;
};
}
