#pragma once

#include <ioj/enum_array.h>
#include <ioj/levels/objective_outcome.h>
#include <ioj/sim/expressions/expr_handle.h>

namespace ioj::sim {
using ObjectiveRoots = EnumArray<ExprHandle, ObjectiveOutcome>;
}
