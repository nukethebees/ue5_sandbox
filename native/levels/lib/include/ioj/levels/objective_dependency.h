#pragma once

#include <ioj/levels/mission_types.h>
#include <ioj/levels/objective_name.h>

namespace ioj::levels {
struct ObjectiveDependency {
    auto operator==(ObjectiveDependency const&) const -> bool = default;

    ObjectiveName objective{};
    ObjectiveOutcome outcome{ObjectiveOutcome::Passed};
};
}
