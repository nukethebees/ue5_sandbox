#pragma once

#include <SpaceGame/levels/LevelDefinition.h>
#include <SpaceGame/simulation/LevelSimulation.h>

class USpaceGameLevelConfig;

namespace ml {
SPACEGAME_API auto
    make_level_simulation_init_data(USpaceGameLevelConfig const& config,
                                    FFixedTickLoop const& clock_settings = {},
                                    TOptional<test_space_ship::FPlayerSpawnData> player = NullOpt)
        -> FLevelSimulationInitData;

SPACEGAME_API auto
    make_level_simulation_init_data(USpaceGameLevelConfig const& config,
                                    FFixedTickLoop const& clock_settings,
                                    FLevelDefinition const& definition,
                                    TOptional<test_space_ship::FPlayerSpawnData> player = NullOpt,
                                    WorldAABBs static_bounds = {}) -> FLevelSimulationInitData;
}
