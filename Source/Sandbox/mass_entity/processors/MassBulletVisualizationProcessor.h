// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MassExecutionContext.h"
#include "MassProcessor.h"
#include "MassQueryExecutor.h"

#include "Sandbox/actor_components/MassBulletVisualizationComponent.h"
#include "Sandbox/mass_entity/fragments/MassBulletFragments.h"
#include "Sandbox/mixins/log_msg_mixin.hpp"
#include "Sandbox/mixins/MassProcessorMixins.hpp"

#include "MassBulletVisualizationProcessor.generated.h"

class UMassBulletVisualizationComponent;

struct FMassBulletVisualizationExecutor
    : public UE::Mass::FQueryExecutor
    , public ml::LogMsgMixin<"FMassBulletVisualizationExecutor"> {
    FMassBulletVisualizationExecutor() = default;

    UE::Mass::FQueryDefinition<UE::Mass::FConstFragmentAccess<FMassBulletTransformFragment>,
                               UE::Mass::FConstFragmentAccess<FMassBulletInstanceIndexFragment>>
        accessors{*this};

    UMassBulletVisualizationComponent* visualization_component{nullptr};

    virtual void Execute(FMassExecutionContext& context) override;
};

UCLASS()
class SANDBOX_API UMassBulletVisualizationProcessor
    : public UMassProcessor
    , public ml::MassProcessorMixins {
    GENERATED_BODY()

    friend struct MassProcessorMixins;
  public:
    void set_visualization_component(UMassBulletVisualizationComponent* component);
    UMassBulletVisualizationProcessor();
  protected:
    UPROPERTY()
    UMassBulletVisualizationComponent* visualization_component{nullptr};
  private:
    FMassEntityQuery entity_query{};
    TSharedPtr<FMassBulletVisualizationExecutor> executor;
};
