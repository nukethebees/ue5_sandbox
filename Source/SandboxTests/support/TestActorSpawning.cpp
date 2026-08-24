#include "TestActorSpawning.h"

#include "SoftTestAssertions.h"

#include <SandboxTests/SandboxTestLogCategories.h>

#include <SpaceGame/simulation/SimulationConfig.h>
#include <SpaceGame/ships/capital/TestCapitalShipProxy.h>
#include <SpaceGame/entities/TestEntity.h>
#include <SpaceGame/simulation/TestSimulationConfig.h>
#include <SpaceGame/ships/player/TestSpaceShip.h>
#include <SpaceGame/ships/player/TestSpaceShipData.h>

#include <Engine/World.h>
#include <Kismet/GameplayStatics.h>

namespace ml {
auto spawn_player_ship(UWorld& world,
                       TSubclassOf<ATestSpaceShip> const player_class,
                       UTestSpaceShipData* const player_config) -> ATestSpaceShip* {
    if (!IsValid(player_class)) {
        UE_LOG(LogSandboxTest,
               Warning,
               TEXT("Cannot spawn player ship: Player ship class is invalid"));
        return nullptr;
    }
    if (!IsValid(player_config)) {
        UE_LOG(LogSandboxTest,
               Warning,
               TEXT("Cannot spawn player ship: Player ship config is invalid"));
        return nullptr;
    }

    auto* const player_ship{
        world.SpawnActorDeferred<ATestSpaceShip>(player_class, FTransform::Identity)};
    if (!IsValid(player_ship)) {
        UE_LOG(LogSandboxTest, Warning, TEXT("Cannot spawn player ship: Deferred spawn failed"));
        return nullptr;
    }

    player_ship->set_actor_config(player_config);
    auto* const finished_actor{
        UGameplayStatics::FinishSpawningActor(player_ship, FTransform::Identity)};
    if (!IsValid(finished_actor)) {
        UE_LOG(LogSandboxTest, Warning, TEXT("Cannot spawn player ship: Finish spawning failed"));
        return nullptr;
    }

    return player_ship;
}

void resolve_proxy_entity_bindings(FProxyEntityMap const& proxy_entities,
                                   TArray<FProxyEntityBinding> const& bindings,
                                   FSoftTestAssertions& checks) {
    for (FProxyEntityBinding const& binding : bindings) {
        if (!checks.is_true(binding.handle != nullptr || binding.unique_id != nullptr,
                            FString::Printf(TEXT("Proxy binding '%s' has an output"),
                                            *binding.test_name.ToString()))) {
            continue;
        }
        if (!checks.is_true(!binding.test_name.IsNone(), TEXT("Proxy binding has a test name"))) {
            continue;
        }

        int32 matches{0};
        FRegistryEntityIdentifiers const* matched_identifiers{nullptr};
        for (auto const& [actor, identifiers] : proxy_entities) {
            auto const* const entity{Cast<ITestEntity>(actor)};
            if (!entity || entity->get_test_name() != binding.test_name) {
                continue;
            }

            ++matches;
            matched_identifiers = &identifiers;
        }

        if (!checks.are_equal(1,
                              matches,
                              FString::Printf(TEXT("Exactly one proxy is named '%s'"),
                                              *binding.test_name.ToString()))) {
            continue;
        }

        check(matched_identifiers);
        if (binding.handle) {
            *binding.handle = matched_identifiers->handle;
        }
        if (binding.unique_id) {
            *binding.unique_id = matched_identifiers->unique_id;
        }
    }
}

auto spawn_capital_proxy(UWorld& world,
                         UTestSimulationConfig const& config,
                         FSoftTestAssertions& checks,
                         FName const test_name,
                         FVector const& location) -> ATestCapitalShipProxy* {
    return spawn_capital_proxy(
        world, config, checks, test_name, FTransform{FRotator::ZeroRotator, location});
}

auto spawn_capital_proxy(UWorld& world,
                         UTestSimulationConfig const& config,
                         FSoftTestAssertions& checks,
                         FName const test_name,
                         FTransform const& transform) -> ATestCapitalShipProxy* {
    auto const proxy_class{config.actor_classes.capital_ship_proxy_class};
    if (!checks.is_true(IsValid(proxy_class), TEXT("Capital proxy actor class is available"))) {
        return nullptr;
    }

    auto* const proxy{world.SpawnActorDeferred<ATestCapitalShipProxy>(proxy_class, transform)};
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
    proxy->set_test_name(test_name);

    auto* finished_actor{UGameplayStatics::FinishSpawningActor(proxy, transform)};
    if (!checks.is_valid(finished_actor, TEXT("Finish spawning succeeded."))) {
        return nullptr;
    }

    return proxy;
}
}
