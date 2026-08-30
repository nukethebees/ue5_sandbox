#pragma once

#include <SpaceGame/entities/ProxyEntityMap.h>
#include <SpaceGame/ships/player/TestSpaceShip.h>

#include <CoreMinimal.h>

class ATestCapitalShipProxy;
class USpaceGameLevelConfig;
struct FPlayerShipConfig;
class UWorld;

namespace ml {
struct FSoftTestAssertions;

struct FProxyEntityBinding {
    FName test_name{NAME_None};
    FRegistryEntityHandle* handle{nullptr};
    TestEntityUniqueId* unique_id{nullptr};
};

void resolve_proxy_entity_bindings(FProxyEntityMap const& proxy_entities,
                                   TArray<FProxyEntityBinding> const& bindings,
                                   FSoftTestAssertions& checks);

auto spawn_player_ship(UWorld& world,
                       TSubclassOf<ATestSpaceShip> const player_class,
                       FPlayerShipConfig const* player_config,
                       FTransform const& transform = FTransform::Identity) -> ATestSpaceShip*;

auto spawn_capital_proxy(UWorld& world,
                         USpaceGameLevelConfig const& config,
                         FSoftTestAssertions& checks,
                         FName const test_name,
                         FVector const& location) -> ATestCapitalShipProxy*;
auto spawn_capital_proxy(UWorld& world,
                         USpaceGameLevelConfig const& config,
                         FSoftTestAssertions& checks,
                         FName const test_name,
                         FTransform const& transform) -> ATestCapitalShipProxy*;
}
