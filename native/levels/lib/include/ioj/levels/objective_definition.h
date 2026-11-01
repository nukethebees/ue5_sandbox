#pragma once

#include <ioj/levels/mission_expr.h>
#include <ioj/levels/objective_roots.h>
#include <ioj/levels/objective_wins_level.h>
#include <ioj/levels/team.h>

#include <string>
#include <vector>

namespace ioj::levels {
struct ObjectiveDefinition {
    auto operator==(ObjectiveDefinition const&) const -> bool = default;

    ObjectiveName id{};
    std::string title{};
    Team team{Team::White};
    ObjectiveWinsLevel wins_level{ObjectiveWinsLevel::No};
    std::vector<MissionExpr> expressions{};
    ObjectiveRoots roots{absent_expression};
    std::vector<ObjectiveDependency> dependencies{};
};
}
