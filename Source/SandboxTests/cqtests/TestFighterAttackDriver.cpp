#include "TestFighterAttackDriver.h"

#include <SandboxTests/SandboxTestLogCategories.h>

#include <SandboxCore/uobject_utils.h>

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
}
void ATestFighterAttackDriver::BeginPlay() {
    Super::BeginPlay();
}
