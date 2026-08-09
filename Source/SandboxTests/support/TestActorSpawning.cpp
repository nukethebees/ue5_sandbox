#include "TestActorSpawning.h"

#include "SoftTestAssertions.h"

#include <Sandbox/batch_game/SimulationConfig.h>
#include <Sandbox/batch_game/TestCapitalShipProxy.h>
#include <Sandbox/batch_game/TestEntity.h>
#include <Sandbox/batch_game/TestSimulationConfig.h>

#include <Engine/World.h>
#include <Kismet/GameplayStatics.h>

namespace ml {
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
    proxy->set_test_name(test_name);

    auto* finished_actor{
        UGameplayStatics::FinishSpawningActor(proxy, FTransform{FRotator::ZeroRotator, location})};
    if (!checks.is_valid(finished_actor, TEXT("Finish spawning succeeded."))) {
        return nullptr;
    }

    return proxy;
}
}
