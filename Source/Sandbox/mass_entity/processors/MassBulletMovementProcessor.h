// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassQueryExecutor.h"

#include "Sandbox/mass_entity/fragments/MassBulletFragments.h"

#include "MassBulletMovementProcessor.generated.h"

struct FMassBulletMovementExecutor : public UE::Mass::FQueryExecutor {
    FMassBulletMovementExecutor() = default;

    UE::Mass::FQueryDefinition<UE::Mass::FMutableFragmentAccess<FMassBulletTransformFragment>,
                               UE::Mass::FConstFragmentAccess<FMassBulletVelocityFragment>>
        accessors{*this};

    virtual void Execute(FMassExecutionContext& context) {
        constexpr auto executor{[](FMassExecutionContext& context, auto& Data, uint32 EntityIndex) {
            auto const delta_time{context.GetDeltaTimeSeconds()};
            auto const transforms{context.GetMutableFragmentView<FMassBulletTransformFragment>()};
            auto const velocities{context.GetFragmentView<FMassBulletVelocityFragment>()};

            auto const n{context.GetNumEntities()};
            for (auto i{0}; i < n; ++i) {
                auto const displacement{velocities[i].velocity * delta_time};
                transforms[i].transform.AddToTranslation(displacement);
            }
        }};

        ForEachEntity(context, accessors, std::move(executor));
    }
};

UCLASS()
class SANDBOX_API UMassBulletMovementProcessor : public UMassProcessor {
    GENERATED_BODY()
  public:
    UMassBulletMovementProcessor() {
        executor =
            UE::Mass::FQueryExecutor::CreateQuery<FMassBulletMovementExecutor>(entity_query, this);
    }
  private:
    FMassEntityQuery entity_query{};
    TSharedPtr<FMassBulletMovementExecutor> executor;
};
