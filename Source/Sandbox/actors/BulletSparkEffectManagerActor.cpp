#include "Sandbox/actors/BulletSparkEffectManagerActor.h"

#include "NiagaraDataInterfaceArrayFunctionLibrary.h"

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
    auto const n{static_cast<int32>(impacts.size())};
    impact_positions.SetNum(n);
    impact_rotations.SetNum(n);

    for (int32 i{0}; i < n; ++i) {
        impact_positions[i] = impacts[i].location;
        impact_rotations[i] = impacts[i].rotation.Quaternion();
    }

    using NL = UNiagaraDataInterfaceArrayFunctionLibrary;

    NL::SetNiagaraArrayVector(niagara_component, FName("ImpactPositions"), impact_positions);
    NL::SetNiagaraArrayQuat(niagara_component, FName("ImpactRotations"), impact_rotations);
}
