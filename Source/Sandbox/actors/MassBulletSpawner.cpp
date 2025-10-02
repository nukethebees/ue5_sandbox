#include "Sandbox/actors/MassBulletSpawner.h"

#include "Components/ArrowComponent.h"

#include "Sandbox/actors/BulletActor.h"
#include "Sandbox/data/pool/PoolConfig.h"
#include "Sandbox/subsystems/world/ObjectPoolSubsystem.h"

AMassBulletSpawner::AMassBulletSpawner() {
    PrimaryActorTick.bCanEverTick = true;

    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    RootComponent->SetMobility(EComponentMobility::Movable);

    fire_point = CreateDefaultSubobject<UArrowComponent>(TEXT("fire_point"));
    fire_point->SetupAttachment(RootComponent);
}
void AMassBulletSpawner::BeginPlay() {
    Super::BeginPlay();
}
void AMassBulletSpawner::Tick(float DeltaTime) {
    Super::Tick(DeltaTime);

    time_since_last_shot += DeltaTime;
    auto const seconds_per_bullet{1.0f / bullets_per_second};

    if (time_since_last_shot >= seconds_per_bullet) {
        spawn_bullet();
        time_since_last_shot = 0.0f;
    }
}

void AMassBulletSpawner::spawn_bullet() {
    TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("Sandbox::AMassBulletSpawner::spawn_bullet"))

    if (!bullet_class) {
        log_warning(TEXT("bullet_class is nullptr."));
        return;
    }
    if (!fire_point) {
        log_warning(TEXT("fire_point is nullptr."));
        return;
    }

    auto const spawn_location{fire_point->GetComponentLocation()};
    auto const spawn_rotation{fire_point->GetComponentRotation()};
}
