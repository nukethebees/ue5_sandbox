#pragma once
#include <ioj/sim/collision/world_aabbs.h>
#include <ioj/sim/levels/level_sim.h>
#include <SpaceGame/entities/ProxyEntityMap.h>
#include <SpaceGame/levels/LevelDefinition.h>
#include <SpaceGame/missions/LevelMissionDefinition.h>
#include <SpaceGameSimulation/levels/LevelStartErrors.h>

#include <expected>

class USpaceGameLevelConfig;
class UStaticMesh;
class UWorld;
class ATestCapitalShipProxy;
class ATestStaticTurretsProxy;
class ATestTubeSpinnerProxy;
class ATestSpaceShip;

namespace ioj {
struct FLevelLaunchOptions;

using FLevelSimBuildResult = std::expected<sim::LevelSimInitData, FLevelStartErrors>;

struct SPACEGAME_API FLevelSimulationBuild {
    auto bind_proxy_entities(sim::LevelSim const& simulation) const -> FProxyEntityMap;
    void destroy_proxy_actors() const;

    sim::LevelSimInitData data;
    TArray<FTransform> initial_turret_transforms;
    TArray<ATestCapitalShipProxy*> capital_proxies;
    TArray<ATestStaticTurretsProxy*> turret_proxies;
    TArray<ATestTubeSpinnerProxy*> spinner_proxies;
};

using FLevelSimulationBuildResult = std::expected<FLevelSimulationBuild, FLevelStartErrors>;

struct FLevelSimulationRequest {
    UWorld& world;
    USpaceGameLevelConfig const& config;
    settings::TickSettings const& clock_settings;
    ::FLevelMissionDefinition& mission;
    FLevelLaunchOptions const& launch_options;
    FString const& level_source;
    FString const& settings_source;
    levels::LevelDefinition const* definition{};
    ATestSpaceShip* player{};
    bool presentation_enabled{true};
};

SPACEGAME_API auto prepare_level_simulation(FLevelSimulationRequest const& request)
    -> FLevelSimulationBuildResult;

SPACEGAME_API void configure_player_actor(ATestSpaceShip& player,
                                          USpaceGameLevelConfig const& config);
SPACEGAME_API void apply_level_actor_configuration(UWorld& world,
                                                   USpaceGameLevelConfig& config,
                                                   ATestSpaceShip* player);

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
