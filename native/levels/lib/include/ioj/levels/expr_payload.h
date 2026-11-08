#pragma once

#include <ioj/levels/all_expr.h>
#include <ioj/levels/any_expr.h>
#include <ioj/levels/entity_destroyed_expr.h>
#include <ioj/levels/entity_kills_reached_expr.h>
#include <ioj/levels/entity_reached_position_expr.h>
#include <ioj/levels/event_fired_expr.h>
#include <ioj/levels/latched_false_expr.h>
#include <ioj/levels/latched_true_expr.h>
#include <ioj/levels/not_expr.h>
#include <ioj/levels/timer_elapsed_expr.h>
#include <ioj/levels/xor_expr.h>

#include <variant>

namespace ioj::levels {
using ExprPayload = std::variant<AllExpr,
                                 AnyExpr,
                                 EntityDestroyedExpr,
                                 EntityReachedPositionExpr,
                                 TimerElapsedExpr,
                                 NotExpr,
                                 XorExpr,
                                 LatchedTrueExpr,
                                 LatchedFalseExpr,
                                 EventFiredExpr,
                                 EntityKillsReachedExpr>;
}
