// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"

#include "MassBulletMovementProcessor.generated.h"

UCLASS()
class SANDBOX_API UMassBulletMovementProcessor : public UMassProcessor {
    GENERATED_BODY()
  public:
    UMassBulletMovementProcessor();
  protected:
    virtual void ConfigureQueries(TSharedRef<FMassEntityManager> const& manager) override;
    virtual void Execute(FMassEntityManager& EntityManager,
                         FMassExecutionContext& Context) override;
  private:
    FMassEntityQuery entity_query{};
};
