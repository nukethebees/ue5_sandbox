#pragma once
#include <ioj/levels/launch_options.h>
#include <ioj/levels/level_definition.h>
#include <ioj/sim/levels/level_sim.h>
#include <ioj/sim/levels/level_simulation_builder.h>
#include <SpaceGameSimulation/levels/LevelStartErrors.h>

#include <expected>

namespace ioj {
class USpaceGameLevelConfig;
class ATestSpaceShip;

using FLevelSimBuildResult = std::expected<sim::LevelSimInitData, FLevelStartErrors>;

struct FLevelSimulationRequest {
    USpaceGameLevelConfig const& config;
    sim::LevelRunTiming timing{};
    levels::LaunchOptions const& launch_options;
    levels::LevelDefinition const& definition;
    ATestSpaceShip* player{};
};

SPACEGAME_API auto prepare_level_simulation(FLevelSimulationRequest const& request)
    -> FLevelSimBuildResult;

}
