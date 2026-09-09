#pragma once

#include <SpaceGame/levels/LevelDefinition.h>
#include <SpaceGameSimulation/levels/LevelStartErrors.h>
#include <SpaceGameSimulation/simulation/LevelSimulation.h>

#include <expected>

class USpaceGameLevelConfig;
class UStaticMesh;

namespace ml {
using FLevelSimulationBuildResult = std::expected<FLevelSimulationInitData, FLevelStartErrors>;

SPACEGAME_API void validate_world_fighter_spawn_slots(FLevelSimulationInitData const& data,
                                                      FLevelStartErrors& errors);

SPACEGAME_API auto
    make_level_simulation_init_data(USpaceGameLevelConfig const& config,
                                    FFixedTickLoop const& clock_settings = {},
                                    TOptional<test_space_ship::FPlayerSpawnData> player = NullOpt,
                                    UStaticMesh const* player_collision_mesh = nullptr)
        -> FLevelSimulationBuildResult;

SPACEGAME_API auto
    make_level_simulation_init_data(USpaceGameLevelConfig const& config,
                                    FFixedTickLoop const& clock_settings,
                                    FLevelDefinition const& definition,
                                    TOptional<test_space_ship::FPlayerSpawnData> player = NullOpt,
                                    WorldAABBs static_bounds = {},
                                    UStaticMesh const* player_collision_mesh = nullptr)
        -> FLevelSimulationBuildResult;
}
