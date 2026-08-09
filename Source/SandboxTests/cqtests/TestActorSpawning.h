#pragma once

#include <Sandbox/batch_game/ProxyEntityMap.h>

#include <CoreMinimal.h>

class ATestCapitalShipProxy;
class UTestSimulationConfig;
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

auto spawn_capital_proxy(UWorld& world,
                         UTestSimulationConfig const& config,
                         FSoftTestAssertions& checks,
                         FName const test_name,
                         FVector const& location) -> ATestCapitalShipProxy*;
}
