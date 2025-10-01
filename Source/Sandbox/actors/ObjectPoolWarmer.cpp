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
        return;
    }

    //// Pre-warm bullet pools
    // for (auto const& config : BulletPoolWarmups) {
    //     if (!config.ActorClass) {
    //         continue;
    //     }
    //
    //     TArray<AActor*> spawned_items{};
    //     spawned_items.Reserve(config.Count);
    //
    //     // Request items to force pool to spawn them
    //     for (int32 i{0}; i < config.Count; ++i) {
    //         if (auto* item{pool->GetItem<FBulletPoolConfig>(config.ActorClass)}) {
    //             spawned_items.Add(item);
    //         }
    //     }
    //
    //     // Return all items to pool
    //     for (auto* item : spawned_items) {
    //         pool->ReturnItem<FBulletPoolConfig>(Cast<ABulletActor>(item));
    //     }
    // }
}
