#include "Sandbox/actors/BulletSparkEffectManagerActor.h"

#include "Sandbox/subsystems/world/BulletSparkEffectSubsystem.h"

ABulletSparkEffectManagerActor::ABulletSparkEffectManagerActor() {
    PrimaryActorTick.bCanEverTick = false;

    niagara_component = CreateDefaultSubobject<UNiagaraComponent>(TEXT("NiagaraComponent"));
    RootComponent = niagara_component;
}

void ABulletSparkEffectManagerActor::BeginPlay() {
    Super::BeginPlay();

    if (auto* subsystem{GetWorld()->GetSubsystem<UBulletSparkEffectSubsystem>()}) {
        subsystem->register_actor(this);
    }
}
