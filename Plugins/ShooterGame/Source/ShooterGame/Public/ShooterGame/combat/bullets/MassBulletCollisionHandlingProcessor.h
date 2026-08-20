// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassQueryExecutor.h"

#include "ShooterGame/combat/bullets/MassBulletFragments.h"
#include "SandboxGameShared/logging/LogMsgMixin.hpp"
#include "ShooterGame/logging/ShooterGameLogCategories.h"
#include "ShooterGame/mass_entity/MassProcessorMixins.hpp"

#include "MassBulletCollisionHandlingProcessor.generated.h"

struct FMassBulletCollisionHandlingExecutor
    : public UE::Mass::FQueryExecutor
    , public ml::LogMsgMixin<"FMassBulletCollisionHandlingExecutor", LogShooterGameMassEntity> {
    FMassBulletCollisionHandlingExecutor() = default;

    using Query = UE::Mass::FQueryDefinition<
        UE::Mass::FConstFragmentAccess<FMassBulletStateFragment>,
        UE::Mass::FConstFragmentAccess<FMassBulletHitInfoFragment>,
        UE::Mass::FConstFragmentAccess<FMassBulletDamageFragment>,
        UE::Mass::FConstSharedFragmentAccess<FMassBulletImpactEffectFragment>>;

    Query accessors{*this};

    virtual void Execute(FMassExecutionContext& context) override;
};

UCLASS()
class SHOOTERGAME_API UMassBulletCollisionHandlingProcessor
    : public UMassProcessor
    , public ml::MassProcessorMixins {
    GENERATED_BODY()

    friend struct MassProcessorMixins;
  public:
    UMassBulletCollisionHandlingProcessor();
  private:
    FMassEntityQuery entity_query{};
    TSharedPtr<FMassBulletCollisionHandlingExecutor> executor;
};
