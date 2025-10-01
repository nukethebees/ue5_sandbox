// Fill out your copyright notice in the Description page of Project Settings.

#include "Sandbox/actors/ObjectPoolWarmer.h"

#include "Sandbox/data/pool/PoolConfig.h"
#include "Sandbox/subsystems/ObjectPoolSubsystem.h"

AObjectPoolWarmer::AObjectPoolWarmer() {
    PrimaryActorTick.bCanEverTick = false;
}

void AObjectPoolWarmer::BeginPlay() {
    TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("Sandbox::AObjectPoolWarmer::BeginPlay"))

    Super::BeginPlay();

    auto* pool{GetWorld()->GetSubsystem<UObjectPoolSubsystem>()};
    if (!pool) {
        log_warning(TEXT("UObjectPoolSubsystem is nullptr."));
        return;
    }

    for (auto const& config : bullet_pool_warmups) {
        pool->preallocate<FBulletPoolConfig>(
            TSubclassOf<typename FBulletPoolConfig::ActorType>(config.actor_class), config.count);
    }
}
