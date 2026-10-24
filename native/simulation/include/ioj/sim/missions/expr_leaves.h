#pragma once

#include <ioj/levels/mission_types.h>
#include <ioj/sim/index_span.h>
#include <ioj/sim/missions/objective_id.h>
#include <ioj/sim/missions/rule_id.h>

#include <vector>

namespace ioj::sim {
struct ExprLeaves {
    std::vector<ExprResult> results;
    std::vector<ObjectiveId> owners;
    std::vector<IndexSpan> parents;
    std::vector<RuleId> rules;
};
}
