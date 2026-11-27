#pragma once

#include <ioj/levels/team.h>

#include <cstdint>
#include <vector>

namespace ioj::levels {
class LevelDefinition;
}
namespace ioj::sim::tests {
struct AuthoringSpawnObservations {
    std::uint32_t initial_capitals;
    std::uint32_t initial_spinners;
    std::uint32_t capitals;
    std::vector<Team> fighter_teams;
};

auto observe_validated_level_spawns(levels::LevelDefinition const& definition, double seconds)
    -> AuthoringSpawnObservations;
}
