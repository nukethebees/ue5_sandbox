#include "sandbox/simulation/fighter_spawn_initialization.h"

#include "sandbox/simulation/fighter_navigation.h"
#include "sandbox/simulation/rotator_math.h"

#include <cassert>
#include <cstddef>

namespace ml::simulation::fighters {
void initialize_spawned_fighters(SpawnInitializationView const fighters,
                                 TestCapitalShipFighterSpawnQueueConstView const spawns,
                                 SpawnInitializationParameters const parameters) {
    spawns.validate_array_sizes();
    auto const count{spawns.num()};
    assert(fighters.locations.num() == count);
    assert(fighters.tasks.size() == static_cast<std::size_t>(count));

    for (std::int32_t index{}; index < count; ++index) {
        auto const element{static_cast<std::size_t>(index)};
        auto const location{spawns.locations[index]};
        auto const rotation{spawns.rotations[index]};
        fighters.tasks[element] = CapitalShipFighterTask::Attack;
        fighters.locations.set(index, location);
        fighters.desired_move_locations.set(index, location);
        fighters.aim_directions.set(index, forward_direction(rotation));
        fighters.speeds[element] = parameters.speed;
        fighters.teams[element] = static_cast<std::byte>(spawns.teams[element]);
        fighters.healths[element] = parameters.health;
        fighters.parents[element] = spawns.parents[element];
        fighters.targets[element] = spawns.targets[element];
        fighters.navigation_risk_tiers[element] =
            static_cast<std::uint8_t>(NavigationRiskTier::Nearby);
        fighters.avoidance_choices[element] = parameters.direct_choice;
        fighters.navigation_periods[element] = parameters.navigation_period;
    }
}
}
