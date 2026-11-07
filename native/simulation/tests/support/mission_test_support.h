#pragma once

#include "simulation_test_support.h"
#include <ioj/sim/levels/level_compilation.h>
#include <ioj/sim/missions/mission.h>

namespace ioj::sim::tests {
inline auto mission_entity(std::uint32_t const index) -> levels::EntityId {
    return levels::EntityId{"entity-" + std::to_string(index)};
}
inline auto destroyed(std::uint32_t const index) -> levels::MissionExpr {
    return {ioj::levels::EntityDestroyedExpr{.entity = mission_entity(index)}};
}
inline void install_mission(SimulationScenario& data, levels::ObjectiveDefinition objective) {
    objective.id = ioj::levels::ObjectiveName{"primary"};
    objective.wins_level = ObjectiveWinsLevel::Yes;
    levels::ObjectiveDefinition opposition{.id = levels::ObjectiveName{"opposition"},
                                           .team = Team::Red,
                                           .wins_level = ObjectiveWinsLevel::Yes};
    opposition.expressions = {{levels::ObjectiveResolvedExpr{
        .objective = {levels::ObjectiveName{"primary"}, ObjectiveOutcome::Failed}}}};
    opposition.roots[ObjectiveOutcome::Passed] = 0;
    levels::MissionDefinition definition{
        .objectives = {std::move(objective), std::move(opposition)}};
    data.mission = std::move(definition);
}
}
