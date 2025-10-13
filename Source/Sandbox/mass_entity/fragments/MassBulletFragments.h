// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTypes.h"

#include "Sandbox/data/health/HealthChange.h"

#include "MassBulletFragments.generated.h"

class UNiagaraSystem;
class AMassBulletVisualizationActor;
struct FHealthChange;

USTRUCT()
struct SANDBOX_API FMassBulletTransformFragment : public FMassFragment {
    GENERATED_BODY()

    FMassBulletTransformFragment() = default;

    UPROPERTY()
    FTransform transform{};
};

USTRUCT()
struct SANDBOX_API FMassBulletVelocityFragment : public FMassFragment {
    GENERATED_BODY()

    FMassBulletVelocityFragment() = default;

    UPROPERTY()
    FVector velocity{FVector::ZeroVector};
};

USTRUCT()
struct SANDBOX_API FMassBulletLastPositionFragment : public FMassFragment {
    GENERATED_BODY()

    FMassBulletLastPositionFragment() = default;

    UPROPERTY()
    FVector last_position{FVector::ZeroVector};
};

USTRUCT()
struct SANDBOX_API FMassBulletHitInfoFragment : public FMassFragment {
    GENERATED_BODY()

    FMassBulletHitInfoFragment() = default;
    FMassBulletHitInfoFragment(FVector const& hit_location,
                               FVector const& hit_normal,
                               TWeakObjectPtr<AActor> const& hit_actor)
        : hit_location(hit_location)
        , hit_normal(hit_normal)
        , hit_actor(hit_actor) {}

    UPROPERTY()
    FVector hit_location{FVector::ZeroVector};

    UPROPERTY()
    FVector hit_normal{FVector::ZeroVector};

    UPROPERTY()
    TWeakObjectPtr<AActor> hit_actor{nullptr};
};

USTRUCT()
struct SANDBOX_API FMassBulletStateFragment : public FMassFragment {
    GENERATED_BODY()

    FMassBulletStateFragment() = default;

    UPROPERTY()
    bool hit_occurred{false};
};

USTRUCT()
struct SANDBOX_API FMassBulletImpactEffectFragment : public FMassConstSharedFragment {
    GENERATED_BODY()

    FMassBulletImpactEffectFragment() = default;
    FMassBulletImpactEffectFragment(UNiagaraSystem* impact_effect)
        : impact_effect(impact_effect) {}

    UPROPERTY()
    UNiagaraSystem* impact_effect{nullptr};
};

USTRUCT()
struct SANDBOX_API FMassBulletVisualizationActorFragment : public FMassConstSharedFragment {
    GENERATED_BODY()

    FMassBulletVisualizationActorFragment() = default;
    FMassBulletVisualizationActorFragment(AMassBulletVisualizationActor* actor)
        : actor(actor) {}

    UPROPERTY()
    AMassBulletVisualizationActor* actor{nullptr};
};

USTRUCT()
struct SANDBOX_API FMassBulletDamageFragment : public FMassConstSharedFragment {
    GENERATED_BODY()

    FMassBulletDamageFragment() = default;
    FMassBulletDamageFragment(FHealthChange const& damage)
        : damage(damage) {}

    UPROPERTY()
    FHealthChange damage{};
};
