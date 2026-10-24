#pragma once

#include <ioj/levels/mission_expression.h>
#include <ioj/levels/objective_roots.h>
#include <ioj/levels/team.h>

#include <string>
#include <vector>

namespace ioj::levels {
struct ObjectiveDefinition {
    ObjectiveName id{};
    std::string title{};
    Team team{Team::White};
    std::vector<MissionExpression> expressions{};
    ObjectiveRoots roots{absent_expression};
    std::vector<ObjectiveDependency> dependencies{};
    auto operator==(ObjectiveDefinition const&) const -> bool = default;
};
}
