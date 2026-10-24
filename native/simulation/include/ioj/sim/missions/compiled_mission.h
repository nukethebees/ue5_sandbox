#pragma once

#include <ioj/levels/mission_definition.h>
#include <ioj/sim/missions/activation_notification.h>
#include <ioj/sim/missions/expr_leaves.h>
#include <ioj/sim/missions/expr_operators.h>
#include <ioj/sim/missions/mission_rules.h>
#include <ioj/sim/missions/mission_team_result.h>
#include <ioj/sim/missions/objectives.h>
#include <ioj/sim/missions/team_result_index.h>

#include <expected>

namespace ioj::sim {
struct SimClock;

struct CompiledMission {
    static auto compile(levels::MissionDefinition const& definition,
                        std::span<levels::EntityId const> entities,
                        SimClock const& clock)
        -> std::expected<CompiledMission, std::vector<std::string>>;

    Objectives objectives{};

    ExprOperators composites{};
    ExprLeaves leaves{};
    MissionRules rules{};
    std::vector<ExprRef> children{};
    std::vector<std::uint32_t> parents{};

    std::vector<ActivationNotification> dependents{};
    std::vector<std::uint32_t> observers{};

    std::vector<MissionTeamResult> team_results{};
    std::vector<TeamResultIndex> result_indices{};
};
}
