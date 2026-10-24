#pragma once

#include <ioj/levels/all_expression.h>
#include <ioj/levels/any_expression.h>
#include <ioj/levels/entity_destroyed_expression.h>
#include <ioj/levels/entity_kills_reached_expression.h>
#include <ioj/levels/entity_reached_position_expression.h>
#include <ioj/levels/objective_resolved_expression.h>
#include <ioj/levels/time_elapsed_expression.h>

#include <variant>

namespace ioj::levels {
using ExpressionPayload = std::variant<AllExpression,
                                       AnyExpression,
                                       EntityDestroyedExpression,
                                       EntityReachedPositionExpression,
                                       TimeElapsedExpression,
                                       ObjectiveResolvedExpression,
                                       EntityKillsReachedExpression>;
}
