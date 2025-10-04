#include "Sandbox/actors/BulletSparkEffectManagerActor.h"

#include "Sandbox/subsystems/world/BulletSparkEffectSubsystem.h"

#include "Sandbox/macros/null_checks.hpp"

ABulletSparkEffectManagerActor::ABulletSparkEffectManagerActor() {
    PrimaryActorTick.bCanEverTick = false;

    niagara_component = CreateDefaultSubobject<UNiagaraComponent>(TEXT("NiagaraComponent"));
    RootComponent = niagara_component;
}

void ABulletSparkEffectManagerActor::BeginPlay() {
    Super::BeginPlay();

    TRY_INIT_PTR(subsystem, GetWorld()->GetSubsystem<UBulletSparkEffectSubsystem>());
    subsystem->register_actor(this);
}

void ABulletSparkEffectManagerActor::consume_impacts(std::span<FSparkEffectTransform> impacts) {
    impact_positions.SetNum(impacts.size());
    impact_rotations.SetNum(impacts.size());

    for (std::size_t i{0}; i < impacts.size(); ++i) {
        impact_positions[i] = impacts[i].location;
        impact_rotations[i] = impacts[i].rotation;
    }

    // niagara_component->SetVectorParameter(FName("ImpactPositions"), impact_positions);
    // niagara_component->SetVectorParameter(FName("ImpactRotations"), impact_rotations);
}
