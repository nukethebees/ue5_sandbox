#include "TestFighterAttackDriver.h"

#include <SandboxTests/SandboxTestLogCategories.h>

#include <SandboxCoreEngine/uobject_utils.h>

#include <Sandbox/batch_game/TestBatchOrchestrator.h>
#include <Sandbox/batch_game/TestCapitalShipProxy.h>

ATestFighterAttackDriver::ATestFighterAttackDriver() {}

void ATestFighterAttackDriver::PostInitializeComponents() {
    Super::PostInitializeComponents();

    auto const check_msg{ml::report_invalid_uobject_ptrs({
        SANDBOX_NAMED_UOBJECT_PTR(hero),
        SANDBOX_NAMED_UOBJECT_PTR(enemy),
    })};

    if (check_msg) {
        setup_error = FString::Printf(
            TEXT("ATestFighterAttackDriver::PostInitializeComponents Uobject ptrs are invalid: %s"),
            *check_msg);
        UE_LOG(LogSandboxTest, Error, TEXT("%s"), *setup_error);

        return;
    }

    hero_team = hero->get_team();
    enemy_team = enemy->get_team();

    ATestBatchOrchestrator::on_proxy_entities_bound.RemoveAll(this);
    ATestBatchOrchestrator::on_proxy_entities_bound.AddUObject(this,
                                                               &ThisClass::bind_proxy_entities);
}
void ATestFighterAttackDriver::BeginPlay() {
    Super::BeginPlay();
}

void ATestFighterAttackDriver::bind_proxy_entities(FProxyEntityMap const& proxy_entities) {
    auto const* const new_hero{proxy_entities.Find(hero.Get())};
    auto const* const new_enemy{proxy_entities.Find(enemy.Get())};

    if (!new_hero && !new_enemy) {
        return;
    }

    check(new_hero);
    check(new_enemy);
    check(new_hero->handle != new_enemy->handle);

    hero_handle = new_hero->handle;
    enemy_handle = new_enemy->handle;

    hero = nullptr;
    enemy = nullptr;

    ATestBatchOrchestrator::on_proxy_entities_bound.RemoveAll(this);
}
