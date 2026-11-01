#pragma once

#include <ioj/levels/objective_definition.h>

#include <vector>

namespace ioj::levels {
// Store authored objectives; compile these into the runtime mission form.
struct MissionDefinition {
    auto operator==(MissionDefinition const&) const -> bool = default;

    std::vector<ObjectiveDefinition> objectives{};
};
}
