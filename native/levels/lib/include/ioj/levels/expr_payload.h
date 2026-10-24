#pragma once

#include <ioj/levels/all_expr.h>
#include <ioj/levels/any_expr.h>
#include <ioj/levels/entity_destroyed_expr.h>
#include <ioj/levels/entity_kills_reached_expr.h>
#include <ioj/levels/entity_reached_position_expr.h>
#include <ioj/levels/objective_resolved_expr.h>
#include <ioj/levels/time_elapsed_expr.h>

#include <variant>

namespace ioj::levels {
using ExprPayload = std::variant<AllExpr,
                                 AnyExpr,
                                 EntityDestroyedExpr,
                                 EntityReachedPositionExpr,
                                 TimeElapsedExpr,
                                 ObjectiveResolvedExpr,
                                 EntityKillsReachedExpr>;
}
