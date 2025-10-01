// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "ObjectPoolWarmer.generated.h"

USTRUCT(BlueprintType)
struct FPoolWarmupConfig {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pool Warming")
    TSubclassOf<AActor> ActorClass{nullptr};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pool Warming")
    int32 Count{50};
};

UCLASS()
class SANDBOX_API AObjectPoolWarmer : public AActor {
    GENERATED_BODY()
  public:
    AObjectPoolWarmer();
  protected:
    virtual void BeginPlay() override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pool Warming")
    TArray<FPoolWarmupConfig> BulletPoolWarmups;
};
