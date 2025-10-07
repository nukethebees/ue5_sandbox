// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassQueryExecutor.h"

#include "Sandbox/mass_entity/fragments/MassBulletFragments.h"
#include "Sandbox/mixins/log_msg_mixin.hpp"
#include "Sandbox/mixins/MassProcessorMixins.hpp"

#include "MassBulletCollisionDetectionProcessor.generated.h"

struct FMassBulletCollisionDetectionExecutor
    : public UE::Mass::FQueryExecutor
    , public ml::LogMsgMixin<"FMassBulletCollisionDetectionExecutor"> {
    FMassBulletCollisionDetectionExecutor() = default;

    using Query = UE::Mass::FQueryDefinition<
        UE::Mass::FMutableFragmentAccess<FMassBulletTransformFragment>,
        UE::Mass::FMutableFragmentAccess<FMassBulletLastPositionFragment>,
        UE::Mass::FConstFragmentAccess<FMassBulletVelocityFragment>,
        UE::Mass::FMutableFragmentAccess<FMassBulletHitInfoFragment>,
        UE::Mass::FMutableFragmentAccess<FMassBulletStateFragment>>;

    Query accessors{*this};

    static constexpr float collision_shape_radius{2.0f};

    virtual void Execute(FMassExecutionContext& context) override;
};

UCLASS()
class SANDBOX_API UMassBulletCollisionDetectionProcessor
    : public UMassProcessor
    , public ml::MassProcessorMixins {
    GENERATED_BODY()

    friend struct MassProcessorMixins;
  public:
    UMassBulletCollisionDetectionProcessor();
  private:
    FMassEntityQuery entity_query{};
    TSharedPtr<FMassBulletCollisionDetectionExecutor> executor;
};
