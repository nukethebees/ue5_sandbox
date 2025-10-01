// Fill out your copyright notice in the Description page of Project Settings.

#include "Sandbox/actors/BulletSpawner.h"

#include "Sandbox/actors/BulletActor.h"
#include "Sandbox/data/pool/PoolConfig.h"
#include "Sandbox/subsystems/ObjectPoolSubsystem.h"

ABulletSpawner::ABulletSpawner() {
    PrimaryActorTick.bCanEverTick = true;
}
void ABulletSpawner::BeginPlay() {
    Super::BeginPlay();
}
void ABulletSpawner::Tick(float DeltaTime) {
    Super::Tick(DeltaTime);

    time_since_last_shot += DeltaTime;
    auto const seconds_per_bullet{1.0f / bullets_per_second};

    if (time_since_last_shot >= seconds_per_bullet) {
        spawn_bullet();
        time_since_last_shot = 0.0f;
    }
}

void ABulletSpawner::spawn_bullet() {
    TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("Sandbox::ABulletSpawner::spawn_bullet"))

    if (!bullet_class || !fire_point) {
        return;
    }

    auto const spawn_location{fire_point->GetComponentLocation()};
    auto const spawn_rotation{fire_point->GetComponentRotation()};

    AActor* bullet{nullptr};

    if (auto* pool{GetWorld()->GetSubsystem<UObjectPoolSubsystem>()}) {
        bullet = pool->GetItem<FBulletPoolConfig>();
    }

    // Fallback to SpawnActor if pool unavailable or exhausted
    if (!bullet) {
        log_warning(TEXT("Failed to spawn a bullet."));

        FActorSpawnParameters spawn_params{};
        spawn_params.Owner = this;
        bullet = GetWorld()->SpawnActor<AActor>(
            bullet_class, spawn_location, spawn_rotation, spawn_params);
    }

    if (bullet) {
        bullet->SetActorLocationAndRotation(spawn_location, spawn_rotation);

        if (auto* movement{bullet->FindComponentByClass<UProjectileMovementComponent>()}) {
            movement->InitialSpeed = bullet_speed;
            movement->MaxSpeed = bullet_speed;
        }
    } else {
        log_warning(TEXT("No bullet to move."));
    }
}
