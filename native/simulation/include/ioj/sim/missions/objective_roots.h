#pragma once

#include <ioj/enum_array.h>
#include <ioj/levels/mission_types.h>
#include <ioj/sim/missions/expr_ref.h>

namespace ioj::sim {
using ObjectiveRoots = EnumArray<ObjectiveOutcome, ExprRef>;
}
