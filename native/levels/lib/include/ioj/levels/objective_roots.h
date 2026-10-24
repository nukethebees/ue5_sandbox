#pragma once

#include <ioj/enum_array.h>
#include <ioj/levels/expr_index.h>
#include <ioj/levels/mission_types.h>

namespace ioj::levels {
using ObjectiveRoots = EnumArray<ObjectiveOutcome, ExprIndex>;
}
