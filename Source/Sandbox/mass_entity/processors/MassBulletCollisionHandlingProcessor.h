// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassQueryExecutor.h"

#include "Sandbox/mass_entity/fragments/MassBulletFragments.h"
#include "Sandbox/mixins/log_msg_mixin.hpp"
#include "Sandbox/mixins/MassProcessorMixins.hpp"

#include "MassBulletCollisionHandlingProcessor.generated.h"

struct FMassBulletCollisionHandlingExecutor
    : public UE::Mass::FQueryExecutor
    , public ml::LogMsgMixin<"FMassBulletCollisionHandlingExecutor"> {
    FMassBulletCollisionHandlingExecutor() = default;

    using Query = UE::Mass::FQueryDefinition<
        UE::Mass::FConstFragmentAccess<FMassBulletStateFragment>,
        UE::Mass::FConstFragmentAccess<FMassBulletHitInfoFragment>,
        UE::Mass::FConstSharedFragmentAccess<FMassBulletImpactEffectFragment>>;

    Query accessors{*this};

    virtual void Execute(FMassExecutionContext& context) override;
};

UCLASS()
class SANDBOX_API UMassBulletCollisionHandlingProcessor
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
