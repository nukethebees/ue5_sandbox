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
class AActor;
class ATestCapitalShipProxy;
class ATestStaticTurretsProxy;
class ATestTubeSpinnerProxy;

namespace ioj {
using FLevelSimBuildResult = std::expected<sim::LevelSimInitData, FLevelStartErrors>;

struct FProxyLevelSimBuild {
    auto bind_proxy_entities(sim::LevelSim const& simulation) const -> FProxyEntityMap;
    void destroy_proxy_actors() const;

    sim::LevelSimInitData data;
    TArray<FTransform> initial_turret_transforms;
    TArray<ATestCapitalShipProxy*> capital_proxies;
    TArray<ATestStaticTurretsProxy*> turret_proxies;
    TArray<ATestTubeSpinnerProxy*> spinner_proxies;
};

using FProxyLevelSimBuildResult = std::expected<FProxyLevelSimBuild, FLevelStartErrors>;

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

SPACEGAME_API auto
    make_proxy_level_simulation_init_data(USpaceGameLevelConfig const& config,
                                          settings::TickSettings const& clock_settings,
                                          UWorld& world,
                                          ::FLevelMissionDefinition& mission_definition,
                                          TOptional<sim::player::PlayerSpawnData> player = NullOpt,
                                          AActor const* player_actor = nullptr,
                                          UStaticMesh const* player_collision_mesh = nullptr)
        -> FProxyLevelSimBuildResult;
}
