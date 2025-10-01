// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"

#include "MassBulletVisualizationProcessor.generated.h"

class UMassBulletVisualizationComponent;

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
