// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassQueryExecutor.h"

#include "Sandbox/mass_entity/fragments/MassBulletFragments.h"
#include "Sandbox/mixins/log_msg_mixin.hpp"
#include "Sandbox/mixins/MassProcessorMixins.hpp"

#include "MassBulletCollisionProcessor.generated.h"

struct FMassBulletCollisionExecutor
    : public UE::Mass::FQueryExecutor
    , public ml::LogMsgMixin<"FMassBulletCollisionExecutor"> {
    FMassBulletCollisionExecutor() = default;

    using Query = UE::Mass::FQueryDefinition<
        UE::Mass::FConstFragmentAccess<FMassBulletTransformFragment>,
        UE::Mass::FMutableFragmentAccess<FMassBulletLastPositionFragment>,
        UE::Mass::FConstFragmentAccess<FMassBulletVelocityFragment>,
        UE::Mass::FMutableFragmentAccess<FMassBulletHitInfoFragment>>;

    Query accessors{*this};

    virtual void Execute(FMassExecutionContext& context) override;
};

UCLASS()
class SANDBOX_API UMassBulletCollisionProcessor
    : public UMassProcessor
    , public ml::MassProcessorMixins {
    GENERATED_BODY()

    friend struct MassProcessorMixins;
  public:
    UMassBulletCollisionProcessor();
  private:
    FMassEntityQuery entity_query{};
    TSharedPtr<FMassBulletCollisionExecutor> executor;
};
