#include "authoring_spawn_observations.h"

#include <ioj/sim/levels/level_sim.h>
#include <ioj/sim/levels/level_simulation_builder.h>

#include <cassert>
#include <memory_resource>
#include <utility>

namespace ioj::sim::tests {
auto observe_validated_level_spawns(levels::LevelDefinition const& definition, double seconds)
    -> AuthoringSpawnObservations {
    auto inputs{make_level_simulation_init_data(definition, {}, {})};
    assert(inputs);
    LevelSim simulation{std::move(*inputs), *std::pmr::new_delete_resource()};
    auto const capitals{simulation.capital_ships().num_instances()};
    auto const spinners{simulation.spinners().num_instances()};

    if (seconds > 0.) {
        simulation.start();
        simulation.advance(seconds);
    }

    auto const teams{simulation.fighters().entities().teams()};
    return {capitals,
            spinners,
            simulation.capital_ships().num_instances(),
            {teams.begin(), teams.end()}};
}
}
