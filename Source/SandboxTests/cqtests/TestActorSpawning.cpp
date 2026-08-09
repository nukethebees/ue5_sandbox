#include "TestActorSpawning.h"

#include "SoftTestAssertions.h"

#include <Sandbox/batch_game/SimulationConfig.h>
#include <Sandbox/batch_game/TestCapitalShipProxy.h>
#include <Sandbox/batch_game/TestSimulationConfig.h>

#include <Engine/World.h>
#include <Kismet/GameplayStatics.h>

namespace ml {
auto spawn_capital_proxy(UWorld& world,
                         UTestSimulationConfig const& config,
                         FSoftTestAssertions& checks,
                         FVector const& location) -> ATestCapitalShipProxy* {
    auto const proxy_class{config.actor_classes.capital_ship_proxy_class};
    if (!checks.is_true(IsValid(proxy_class), TEXT("Capital proxy actor class is available"))) {
        return nullptr;
    }

    auto* const proxy{world.SpawnActorDeferred<ATestCapitalShipProxy>(
        proxy_class, FTransform{FRotator::ZeroRotator, location})};
    if (!checks.is_valid(proxy, TEXT("Deferred capital proxy is spawned"))) {
        return nullptr;
    }

    if (!checks.not_nullptr(config.simulation_config.Get(),
                            TEXT("Simulation config is available"))) {
        return nullptr;
    }
    auto* const capital_config{config.simulation_config->capital_ships_config.Get()};
    if (!checks.not_nullptr(capital_config, TEXT("Capital ships config is available"))) {
        return nullptr;
    }
    proxy->set_actor_config(capital_config);

    auto* finished_actor{
        UGameplayStatics::FinishSpawningActor(proxy, FTransform{FRotator::ZeroRotator, location})};
    if (!checks.is_valid(finished_actor, TEXT("Finish spawning succeeded."))) {
        return nullptr;
    }

    return proxy;
}
}
