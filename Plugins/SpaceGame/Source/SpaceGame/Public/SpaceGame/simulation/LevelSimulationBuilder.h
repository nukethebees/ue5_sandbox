#pragma once
#include <ioj/sim/level_sim.h>
#include <ioj/sim/world_aabbs.h>
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

namespace ml {
using FLevelSimBuildResult = std::expected<::ioj::sim::LevelSimInitData, FLevelStartErrors>;

struct FProxyLevelSimBuild {
    ::ioj::sim::LevelSimInitData data;
    TArray<FTransform> initial_turret_transforms;
    TArray<ATestCapitalShipProxy*> capital_proxies;
    TArray<ATestStaticTurretsProxy*> turret_proxies;
    TArray<ATestTubeSpinnerProxy*> spinner_proxies;

    auto bind_proxy_entities(::ioj::sim::LevelSim const& simulation) const -> FProxyEntityMap;
    void destroy_proxy_actors() const;
};

using FProxyLevelSimBuildResult = std::expected<FProxyLevelSimBuild, FLevelStartErrors>;

SPACEGAME_API void validate_world_fighter_spawn_slots(::ioj::sim::LevelSimInitData const& data,
                                                      FLevelStartErrors& errors);

SPACEGAME_API auto
    make_level_simulation_init_data(USpaceGameLevelConfig const& config,
                                    ::ioj::sim::FixedTickLoop const& clock_settings = {},
                                    TOptional<::ioj::sim::player::PlayerSpawnData> player = NullOpt,
                                    UStaticMesh const* player_collision_mesh = nullptr)
        -> FLevelSimBuildResult;

SPACEGAME_API auto
    make_level_simulation_init_data(USpaceGameLevelConfig const& config,
                                    ::ioj::sim::FixedTickLoop const& clock_settings,
                                    FLevelDefinition const& definition,
                                    TOptional<::ioj::sim::player::PlayerSpawnData> player = NullOpt,
                                    ::ioj::sim::collision::WorldAABBs static_bounds = {},
                                    UStaticMesh const* player_collision_mesh = nullptr)
        -> FLevelSimBuildResult;

SPACEGAME_API auto make_proxy_level_simulation_init_data(
    USpaceGameLevelConfig const& config,
    ::ioj::sim::FixedTickLoop const& clock_settings,
    UWorld& world,
    ::FLevelMissionDefinition& mission_definition,
    TOptional<::ioj::sim::player::PlayerSpawnData> player = NullOpt,
    AActor const* player_actor = nullptr,
    UStaticMesh const* player_collision_mesh = nullptr) -> FProxyLevelSimBuildResult;
}
