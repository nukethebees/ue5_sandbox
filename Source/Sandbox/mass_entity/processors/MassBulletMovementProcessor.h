// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassQueryExecutor.h"

#include "Sandbox/mass_entity/fragments/MassBulletFragments.h"
#include "Sandbox/mixins/log_msg_mixin.hpp"
#include "Sandbox/mixins/MassProcessorMixins.hpp"

#include "MassBulletMovementProcessor.generated.h"

struct FMassBulletMovementExecutor
    : public UE::Mass::FQueryExecutor
    , public ml::LogMsgMixin<"FMassBulletMovementExecutor"> {
    FMassBulletMovementExecutor() = default;

    using Query =
        UE::Mass::FQueryDefinition<UE::Mass::FMutableFragmentAccess<FMassBulletTransformFragment>,
                                   UE::Mass::FConstFragmentAccess<FMassBulletVelocityFragment>,
                                   UE::Mass::FConstFragmentAccess<FMassBulletHitOccurredFragment>>;

    Query accessors{*this};

    virtual void Execute(FMassExecutionContext& context) override;
};

UCLASS()
class SANDBOX_API UMassBulletMovementProcessor
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
