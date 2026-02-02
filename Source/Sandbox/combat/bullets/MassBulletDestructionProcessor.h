// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassQueryExecutor.h"

#include "Sandbox/combat/bullets/MassBulletFragments.h"
#include "Sandbox/logging/LogMsgMixin.hpp"
#include "Sandbox/logging/SandboxLogCategories.h"
#include "Sandbox/mass_entity/MassProcessorMixins.hpp"

#include "MassBulletDestructionProcessor.generated.h"

struct FMassBulletDestructionExecutor
    : public UE::Mass::FQueryExecutor
    , public ml::LogMsgMixin<"FMassBulletDestructionExecutor", LogSandboxMassEntity> {
    FMassBulletDestructionExecutor() = default;

    using Query =
        UE::Mass::FQueryDefinition<UE::Mass::FConstFragmentAccess<FMassBulletStateFragment>,
                                   UE::Mass::FConstSharedFragmentAccess<FMassBulletDataFragment>>;

    Query accessors{*this};

    virtual void Execute(FMassExecutionContext& context) override;
};

UCLASS()
class SANDBOX_API UMassBulletDestructionProcessor
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
