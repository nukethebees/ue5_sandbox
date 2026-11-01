#pragma once

#include <ioj/levels/mission_definition.h>
#include <ioj/sim/missions/activation_notification.h>
#include <ioj/sim/missions/expr_composite_index.h>
#include <ioj/sim/missions/expr_handle.h>
#include <ioj/sim/missions/expr_leaf_handle.h>
#include <ioj/sim/missions/expr_leaves.h>
#include <ioj/sim/missions/expr_operators.h>
#include <ioj/sim/missions/objectives.h>

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
    std::vector<ExprHandle> children{};

    std::vector<ActivationNotification> dependents{};
};
}
