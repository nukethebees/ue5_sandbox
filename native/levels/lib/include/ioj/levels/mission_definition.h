#pragma once

#include <ioj/levels/objective_definition.h>
#include <ioj/levels/team_result_definition.h>

#include <vector>

namespace ioj::levels {
// Store authored objectives and team results; compile these into the runtime mission form.
struct MissionDefinition {
    std::vector<ObjectiveDefinition> objectives{};
    std::vector<TeamResultDefinition> team_results{};
    auto operator==(MissionDefinition const&) const -> bool = default;
};
}
