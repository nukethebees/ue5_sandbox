// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MassExecutionContext.h"
#include "MassProcessor.h"
#include "MassQueryExecutor.h"

#include "Sandbox/actor_components/MassBulletVisualizationComponent.h"
#include "Sandbox/data/mass/MassBulletFragments.h"

#include "MassBulletVisualizationProcessor.generated.h"

class UMassBulletVisualizationComponent;

struct FMassBulletVisualizationExecutor : public UE::Mass::FQueryExecutor {
    FMassBulletVisualizationExecutor() = default;

    UE::Mass::FQueryDefinition<UE::Mass::FConstFragmentAccess<FMassBulletTransformFragment>,
                               UE::Mass::FConstFragmentAccess<FMassBulletInstanceIndexFragment>>
        accessors{*this};

    UMassBulletVisualizationComponent* visualization_component{nullptr};

    virtual void Execute(FMassExecutionContext& context) {
        auto executor{[this](FMassExecutionContext& context, auto& Data, uint32 EntityIndex) {
            if (!visualization_component) {
                return;
            }

            auto const transforms{context.GetFragmentView<FMassBulletTransformFragment>()};
            auto const indices{context.GetFragmentView<FMassBulletInstanceIndexFragment>()};

            auto const n{context.GetNumEntities()};
            for (auto i{0}; i < n; ++i) {
                visualization_component->update_instance(indices[i].instance_index,
                                                         transforms[i].transform);
            }
        }};

        ForEachEntity(context, accessors, std::move(executor));
    }
};

UCLASS()
class SANDBOX_API UMassBulletVisualizationProcessor : public UMassProcessor {
    GENERATED_BODY()
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
