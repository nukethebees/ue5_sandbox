#pragma once

#include <ioj/levels/objective_dependency.h>
#include <ioj/levels/team.h>

namespace ioj::levels {
struct TeamResultDefinition {
    auto operator==(TeamResultDefinition const&) const -> bool = default;

    Team team{Team::White};
    ObjectiveDependency when{};
    TeamOutcome result{TeamOutcome::Victory};
};
}
