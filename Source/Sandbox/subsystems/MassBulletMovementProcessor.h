// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassQueryExecutor.h"

#include "Sandbox/data/mass/MassBulletFragments.h"

#include "MassBulletMovementProcessor.generated.h"

struct FMassBulletMovementExecutor : public UE::Mass::FQueryExecutor {
    FMassBulletMovementExecutor() = default;

    UE::Mass::FQueryDefinition<UE::Mass::FMutableFragmentAccess<FMassBulletTransformFragment>,
                               UE::Mass::FConstFragmentAccess<FMassBulletVelocityFragment>>
        Accessors{*this};

    virtual void Execute(FMassExecutionContext& Context) {
        constexpr auto executor{[](FMassExecutionContext& Context, auto& Data, uint32 EntityIndex) {
            auto const delta_time{Context.GetDeltaTimeSeconds()};
            auto const transforms{Context.GetMutableFragmentView<FMassBulletTransformFragment>()};
            auto const velocities{Context.GetFragmentView<FMassBulletVelocityFragment>()};

            for (auto i{0}; i < Context.GetNumEntities(); ++i) {
                auto const displacement{velocities[i].velocity * delta_time};
                transforms[i].transform.AddToTranslation(displacement);
            }
        }};

        ForEachEntity(Context, Accessors, std::move(executor));
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
