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

    if (!check_msg.IsEmpty()) {
        setup_error = FString::Printf(
            TEXT("ATestFighterAttackDriver::PostInitializeComponents Uobject ptrs are invalid: %s"),
            *check_msg);
        UE_LOG(LogSandboxTest, Error, TEXT("%s"), *setup_error);

        return;
    }

    hero_team = hero->get_team();
    enemy_team = enemy->get_team();

    ATestBatchOrchestrator::on_proxy_handles_bound.RemoveAll(this);
    ATestBatchOrchestrator::on_proxy_handles_bound.AddUObject(this, &ThisClass::bind_proxy_handles);
}
void ATestFighterAttackDriver::BeginPlay() {
    Super::BeginPlay();
}

void ATestFighterAttackDriver::bind_proxy_handles(FProxyEntityHandleMap const& proxy_handles) {
    auto const* const new_hero_handle{proxy_handles.Find(hero.Get())};
    auto const* const new_enemy_handle{proxy_handles.Find(enemy.Get())};

    if (!new_hero_handle && !new_enemy_handle) {
        return;
    }

    check(new_hero_handle);
    check(new_enemy_handle);
    check(*new_hero_handle != *new_enemy_handle);

    hero_handle = *new_hero_handle;
    enemy_handle = *new_enemy_handle;

    hero = nullptr;
    enemy = nullptr;

    ATestBatchOrchestrator::on_proxy_handles_bound.RemoveAll(this);
}
