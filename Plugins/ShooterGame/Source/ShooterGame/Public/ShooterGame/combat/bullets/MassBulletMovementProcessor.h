// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassQueryExecutor.h"

#include "ShooterGame/combat/bullets/MassBulletFragments.h"
#include "SandboxGameShared/logging/LogMsgMixin.hpp"
#include "ShooterGame/logging/ShooterGameLogCategories.h"
#include "ShooterGame/mass_entity/MassProcessorMixins.hpp"

#include "MassBulletMovementProcessor.generated.h"

struct FMassBulletMovementExecutor
    : public UE::Mass::FQueryExecutor
    , public ml::LogMsgMixin<"FMassBulletMovementExecutor", LogShooterGameMassEntity> {
    FMassBulletMovementExecutor() = default;

    using Query =
        UE::Mass::FQueryDefinition<UE::Mass::FMutableFragmentAccess<FMassBulletTransformFragment>,
                                   UE::Mass::FConstFragmentAccess<FMassBulletVelocityFragment>,
                                   UE::Mass::FConstFragmentAccess<FMassBulletStateFragment>>;

    Query accessors{*this};

    virtual void Execute(FMassExecutionContext& context) override;
};

UCLASS()
class SHOOTERGAME_API UMassBulletMovementProcessor
    : public UMassProcessor
    , public ml::MassProcessorMixins {
    GENERATED_BODY()

    friend struct MassProcessorMixins;
  public:
    UMassBulletMovementProcessor();
  private:
    FMassEntityQuery entity_query{};
    TSharedPtr<FMassBulletMovementExecutor> executor;
};
