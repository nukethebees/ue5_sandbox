#pragma once
#include <ioj/levels/launch_options.h>
#include <ioj/levels/level_definition.h>
#include <ioj/sim/collision/world_aabbs.h>
#include <ioj/sim/levels/level_sim.h>
#include <SpaceGameSimulation/levels/LevelStartErrors.h>

#include <expected>

class UStaticMesh;
class UWorld;

namespace ioj {
class USpaceGameLevelConfig;
class ATestSpaceShip;

using FLevelSimBuildResult = std::expected<sim::LevelSimInitData, FLevelStartErrors>;

struct FLevelSimulationRequest {
    USpaceGameLevelConfig const& config;
    settings::TickSettings const& clock_settings;
    levels::LaunchOptions const& launch_options;
    levels::LevelDefinition const& definition;
    ATestSpaceShip* player{};
};

SPACEGAME_API auto prepare_level_simulation(FLevelSimulationRequest const& request)
    -> FLevelSimBuildResult;

SPACEGAME_API auto configure_level_run(sim::LevelSimInitData& data,
                                       levels::LaunchOptions const& options)
    -> std::expected<void, FLevelStartErrors>;

SPACEGAME_API void configure_player_actor(ATestSpaceShip& player,
                                          USpaceGameLevelConfig const& config);

SPACEGAME_API auto
    make_level_simulation_init_data(USpaceGameLevelConfig const& config,
                                    settings::TickSettings const& clock_settings = {},
                                    TOptional<sim::player::PlayerSpawnData> player = NullOpt,
                                    UStaticMesh const* player_collision_mesh = nullptr)
        -> FLevelSimBuildResult;

SPACEGAME_API auto
    make_level_simulation_init_data(USpaceGameLevelConfig const& config,
                                    settings::TickSettings const& clock_settings,
                                    levels::LevelDefinition const& definition,
                                    TOptional<sim::player::PlayerSpawnData> player = NullOpt,
                                    sim::collision::WorldAABBs static_bounds = {},
                                    UStaticMesh const* player_collision_mesh = nullptr)
        -> FLevelSimBuildResult;

}
