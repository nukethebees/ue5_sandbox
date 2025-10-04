// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassQueryExecutor.h"

#include "Sandbox/mass_entity/fragments/MassBulletFragments.h"
#include "Sandbox/mixins/log_msg_mixin.hpp"
#include "Sandbox/mixins/MassProcessorMixins.hpp"

#include "MassBulletCleanupProcessor.generated.h"

class UNiagaraSystem;

struct FMassBulletCleanupExecutor
    : public UE::Mass::FQueryExecutor
    , public ml::LogMsgMixin<"FMassBulletCleanupExecutor"> {
    FMassBulletCleanupExecutor() = default;

    using Query = UE::Mass::FQueryDefinition<
        UE::Mass::FConstFragmentAccess<FMassBulletInstanceIndexFragment>,
        UE::Mass::FConstFragmentAccess<FMassBulletHitInfoFragment>,
        UE::Mass::FConstSharedFragmentAccess<FMassBulletImpactEffectFragment>,
        UE::Mass::FConstSharedFragmentAccess<FMassBulletVisualizationActorFragment>,
        UE::Mass::FMassTagRequired<FMassBulletDeadTag>,
        UE::Mass::FMassTagBlocked<FMassBulletActiveTag>>;

    Query accessors{*this};

    virtual void Execute(FMassExecutionContext& context) override;
};

UCLASS()
class SANDBOX_API UMassBulletCleanupProcessor
    : public UMassProcessor
    , public ml::MassProcessorMixins {
    GENERATED_BODY()

    friend struct MassProcessorMixins;
  public:
    UMassBulletCleanupProcessor();
  private:
    FMassEntityQuery entity_query{};
    TSharedPtr<FMassBulletCleanupExecutor> executor;
};
