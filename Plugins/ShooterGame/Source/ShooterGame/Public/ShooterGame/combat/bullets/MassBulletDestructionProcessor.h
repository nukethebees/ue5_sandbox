// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassQueryExecutor.h"

#include "ShooterGame/combat/bullets/MassBulletFragments.h"
#include "SandboxGameShared/logging/LogMsgMixin.hpp"
#include "ShooterGame/logging/ShooterGameLogCategories.h"
#include "ShooterGame/mass_entity/MassProcessorMixins.hpp"

#include "MassBulletDestructionProcessor.generated.h"

struct FMassBulletDestructionExecutor
    : public UE::Mass::FQueryExecutor
    , public ml::LogMsgMixin<"FMassBulletDestructionExecutor", LogShooterGameMassEntity> {
    FMassBulletDestructionExecutor() = default;

    using Query =
        UE::Mass::FQueryDefinition<UE::Mass::FConstFragmentAccess<FMassBulletStateFragment>,
                                   UE::Mass::FConstSharedFragmentAccess<FMassBulletDataFragment>>;

    Query accessors{*this};

    virtual void Execute(FMassExecutionContext& context) override;
};

UCLASS()
class SHOOTERGAME_API UMassBulletDestructionProcessor
    : public UMassProcessor
    , public ml::MassProcessorMixins {
    GENERATED_BODY()

    friend struct MassProcessorMixins;
  public:
    UMassBulletDestructionProcessor();
  private:
    FMassEntityQuery entity_query{};
    TSharedPtr<FMassBulletDestructionExecutor> executor;
};
