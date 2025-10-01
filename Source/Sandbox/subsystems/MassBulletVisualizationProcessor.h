// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassQueryExecutor.h"

#include "Sandbox/data/mass/MassBulletFragments.h"

#include "MassBulletVisualizationProcessor.generated.h"

class UMassBulletVisualizationComponent;

struct FMassBulletVisualizationExecutor : public UE::Mass::FQueryExecutor {
    FMassBulletVisualizationExecutor() = default;

    UE::Mass::FQueryDefinition<UE::Mass::FConstFragmentAccess<FMassBulletTransformFragment>,
                               UE::Mass::FConstFragmentAccess<FMassBulletInstanceIndexFragment>>
        Accessors{*this};

    UMassBulletVisualizationComponent* visualization_component{nullptr};

    virtual void Execute(FMassExecutionContext& Context) {
        constexpr auto executor{[](FMassExecutionContext& Context, auto& Data, uint32 EntityIndex) {

        }};

        ForEachEntity(Context, Accessors, std::move(executor));
    }
};

// UCLASS()
// class SANDBOX_API UMassBulletMovementProcessor : public UMassProcessor {
//     GENERATED_BODY()
// public:
//     UMassBulletMovementProcessor() {
//         executor =
//             UE::Mass::FQueryExecutor::CreateQuery<FMassBulletMovementExecutor>(entity_query,
//             this);
//     }
// private:
//     FMassEntityQuery entity_query{};
//     TSharedPtr<FMassBulletMovementExecutor> executor;
// };

UCLASS()
class SANDBOX_API UMassBulletVisualizationProcessor : public UMassProcessor {
    GENERATED_BODY()
  public:
    UMassBulletVisualizationProcessor();

    void set_visualization_component(UMassBulletVisualizationComponent* component);
  protected:
    virtual void ConfigureQueries(TSharedRef<FMassEntityManager> const& manager) override;
    virtual void Execute(FMassEntityManager& EntityManager,
                         FMassExecutionContext& Context) override;
  private:
    FMassEntityQuery entity_query{};

    UPROPERTY()
    UMassBulletVisualizationComponent* visualization_component{nullptr};
};
