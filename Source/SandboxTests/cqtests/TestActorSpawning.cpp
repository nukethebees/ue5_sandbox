#include "TestActorSpawning.h"

#include <Sandbox/batch_game/SimulationConfig.h>
#include <Sandbox/batch_game/TestCapitalShipProxy.h>
#include <Sandbox/batch_game/TestSimulationConfig.h>

#include <Engine/World.h>
#include <Kismet/GameplayStatics.h>

namespace ml {
auto spawn_capital_proxy(UWorld& world,
                         UTestSimulationConfig const& config,
                         FVector const& location) -> ATestCapitalShipProxy& {
    auto* const proxy{world.SpawnActorDeferred<ATestCapitalShipProxy>(
        ATestCapitalShipProxy::StaticClass(), FTransform{FRotator::ZeroRotator, location})};
    check(proxy);

    auto* const capital_config{config.simulation_config->capital_ships_config.Get()};
    check(capital_config);
    proxy->set_actor_config(capital_config);

    UGameplayStatics::FinishSpawningActor(proxy, FTransform{FRotator::ZeroRotator, location});
    return *proxy;
}
}
