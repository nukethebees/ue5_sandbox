// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MassExecutionContext.h"
#include "MassProcessor.h"
#include "MassQueryExecutor.h"

#include "ShooterGame/combat/bullets/MassBulletFragments.h"
#include "SandboxGameShared/logging/LogMsgMixin.hpp"
#include "ShooterGame/logging/ShooterGameLogCategories.h"
#include "ShooterGame/mass_entity/MassProcessorMixins.hpp"

#include "MassBulletVisualizationProcessor.generated.h"

struct FMassBulletVisualizationExecutor
    : public UE::Mass::FQueryExecutor
    , public ml::LogMsgMixin<"FMassBulletVisualizationExecutor", LogShooterGameMassEntity> {
    FMassBulletVisualizationExecutor() = default;

    using Query = UE::Mass::FQueryDefinition<
        UE::Mass::FConstFragmentAccess<FMassBulletTransformFragment>,
        UE::Mass::FConstSharedFragmentAccess<FMassBulletVisualizationActorFragment>,
        UE::Mass::FConstSharedFragmentAccess<FMassBulletDataFragment>,
        UE::Mass::FConstFragmentAccess<FMassBulletStateFragment>>;

    Query accessors{*this};

    virtual void Execute(FMassExecutionContext& context) override;
};

UCLASS()
class SHOOTERGAME_API UMassBulletVisualizationProcessor
    : public UMassProcessor
    , public ml::MassProcessorMixins {
    GENERATED_BODY()

    friend struct MassProcessorMixins;
  public:
    UMassBulletVisualizationProcessor();
  private:
    FMassEntityQuery entity_query{};
    TSharedPtr<FMassBulletVisualizationExecutor> executor;
};
