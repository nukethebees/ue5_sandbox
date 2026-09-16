#pragma once

#include <ioj/sim/agent_display_batch.h>
#include <ioj/sim/vectors3f.h>

#include <cstdint>
#include <vector>

namespace ml::tests {
struct FDisplayEntityTestData {
    void add_defaulted(std::int32_t const count) {
        locations.add_defaulted(count);
        velocities.add_defaulted(count);
        healths.resize(healths.size() + static_cast<std::size_t>(count));
        teams.resize(teams.size() + static_cast<std::size_t>(count));
        entity_types.resize(entity_types.size() + static_cast<std::size_t>(count));
    }
    auto num() const noexcept -> std::int32_t { return static_cast<std::int32_t>(healths.size()); }

    ::ioj::sim::Vectors3f locations;
    ::ioj::sim::Vectors3f velocities;
    std::vector<::ioj::sim::Health> healths;
    std::vector<::ioj::sim::Team> teams;
    std::vector<::ioj::sim::EntityType> entity_types;
};
}
