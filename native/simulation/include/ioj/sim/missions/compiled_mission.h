#pragma once

#include <ioj/sim/missions/activation_notification.h>
#include <ioj/sim/missions/mission_composites.h>
#include <ioj/sim/missions/mission_leaves.h>
#include <ioj/sim/missions/mission_rules.h>
#include <ioj/sim/missions/mission_team_result.h>
#include <ioj/sim/missions/objectives.h>

namespace ioj::sim {
struct CompiledMission {
    Objectives objectives;
    MissionComposites composites;
    MissionLeaves leaves;
    MissionRules rules;
    std::vector<ExprRef> children;
    std::vector<std::uint32_t> parents;
    std::vector<ActivationNotification> dependents;
    std::vector<std::uint32_t> observers;
    std::vector<MissionTeamResult> team_results;
    std::vector<std::uint32_t> result_indices;
};
}
