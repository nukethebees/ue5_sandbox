#pragma once

#include <ioj/sim/index_span.h>
#include <ioj/sim/missions/condition_type.h>

namespace ioj::sim {
struct RuleRun {
    ConditionType type{};
    IndexSpan rules;
};
}
