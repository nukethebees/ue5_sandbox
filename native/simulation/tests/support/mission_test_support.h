#include <ioj/sim/missions/mission_compilation.h>
#pragma once
#include "simulation_test_support.h"
#include <ioj/sim/missions/mission.h>

namespace ioj::sim::tests {
inline auto mission_entity(std::uint32_t const index) -> levels::EntityId {
    return levels::EntityId{"entity-" + std::to_string(index)};
}
inline auto destroyed(std::uint32_t const index) -> levels::MissionExpression {
    return {ioj::levels::EntityDestroyedExpression{.entity = mission_entity(index)}};
}
inline void install_mission(LevelSimInitData& data, levels::ObjectiveDefinition objective) {
    objective.id = ioj::levels::ObjectiveName{"primary"};
    auto const team{objective.team};
    levels::MissionDefinition definition{
        .objectives = {std::move(objective)},
        .team_results = {{team,
                          {ioj::levels::ObjectiveName{"primary"}, ObjectiveOutcome::Passed},
                          TeamOutcome::Victory},
                         {team,
                          {ioj::levels::ObjectiveName{"primary"}, ObjectiveOutcome::Failed},
                          TeamOutcome::Defeat}}};
    std::vector<levels::EntityId> names;
    auto const count{data.level_events.initialisation.entity_count};
    for (std::uint32_t index{}; index < count; ++index) {
        names.push_back(mission_entity(index));
    }
    SimClock clock;
    clock.initialise(data.clock_settings);
    auto compiled{compile_mission(definition, names, clock)};
    ASSERT_TRUE(compiled.has_value());
    auto& mission{data.level_events.initialisation.mission.emplace()};
    mission.definition = std::move(*compiled);
    mission.save_results = false;
}
}
