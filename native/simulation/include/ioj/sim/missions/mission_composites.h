#pragma once

#include <ioj/levels/mission_types.h>
#include <ioj/sim/index_span.h>
#include <ioj/sim/missions/objective_id.h>

#include <vector>

namespace ioj::sim {
struct MissionComposites {
    std::vector<ExprResult> results;
    std::vector<ExpressionType> types;
    std::vector<ObjectiveId> owners;
    std::vector<IndexSpan> parents;
    std::vector<IndexSpan> children;
    std::vector<std::uint32_t> remaining;
};
}
