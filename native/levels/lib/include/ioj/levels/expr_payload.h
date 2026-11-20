#pragma once

#include <ioj/levels/all_expr.h>
#include <ioj/levels/any_expr.h>
#include <ioj/levels/logic_payloads.h>
#include <ioj/levels/not_expr.h>
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
                                 EntityKillsReachedExpr>;
}
