#pragma once
#include <SGCollision/world_aabbs.h>
#include <SpaceGameSimulation/support/FixedTickLoop.h>

#include <ioj/sim/level_sim.h>
#include <SpaceGame/levels/LevelDefinition.h>
#include <SpaceGameSimulation/levels/LevelStartErrors.h>

#include <expected>

class USpaceGameLevelConfig;
class UStaticMesh;

namespace ml {
using FLevelSimBuildResult = std::expected<::ioj::sim::LevelSimInitData, FLevelStartErrors>;

SPACEGAME_API void validate_world_fighter_spawn_slots(::ioj::sim::LevelSimInitData const& data,
                                                      FLevelStartErrors& errors);

SPACEGAME_API auto
    make_level_simulation_init_data(USpaceGameLevelConfig const& config,
                                    FFixedTickLoop const& clock_settings = {},
                                    TOptional<::ioj::sim::player::PlayerSpawnData> player = NullOpt,
                                    UStaticMesh const* player_collision_mesh = nullptr)
        -> FLevelSimBuildResult;

SPACEGAME_API auto
    make_level_simulation_init_data(USpaceGameLevelConfig const& config,
                                    FFixedTickLoop const& clock_settings,
                                    FLevelDefinition const& definition,
                                    TOptional<::ioj::sim::player::PlayerSpawnData> player = NullOpt,
                                    WorldAABBs static_bounds = {},
                                    UStaticMesh const* player_collision_mesh = nullptr)
        -> FLevelSimBuildResult;
}
