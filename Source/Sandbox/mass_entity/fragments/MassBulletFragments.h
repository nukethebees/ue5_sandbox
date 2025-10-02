// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTypes.h"

#include "MassBulletFragments.generated.h"

class UMassBulletVisualizationComponent;

USTRUCT()
struct SANDBOX_API FMassBulletVisualizationComponentFragment : public FMassFragment {
    GENERATED_BODY()

    TObjectPtr<UMassBulletVisualizationComponent> component{nullptr};
};

USTRUCT()
struct SANDBOX_API FMassBulletTransformFragment : public FMassFragment {
    GENERATED_BODY()

    FTransform transform{};
};

USTRUCT()
struct SANDBOX_API FMassBulletVelocityFragment : public FMassFragment {
    GENERATED_BODY()

    FVector velocity{FVector::ZeroVector};
};

USTRUCT()
struct SANDBOX_API FMassBulletInstanceIndexFragment : public FMassFragment {
    GENERATED_BODY()

    int32 instance_index{-1};
};
