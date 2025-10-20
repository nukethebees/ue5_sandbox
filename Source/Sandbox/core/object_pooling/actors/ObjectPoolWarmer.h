// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "Sandbox/logging/mixins/LogMsgMixin.hpp"

#include "ObjectPoolWarmer.generated.h"

USTRUCT(BlueprintType)
struct FPoolWarmupConfig {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pool Warming")
    TSubclassOf<AActor> actor_class{nullptr};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pool Warming")
    int32 count{50};
};

UCLASS()
class SANDBOX_API AObjectPoolWarmer
    : public AActor
    , public ml::LogMsgMixin<"AObjectPoolWarmer"> {
    GENERATED_BODY()
  public:
    AObjectPoolWarmer();
  protected:
    virtual void BeginPlay() override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pool Warming")
    TArray<FPoolWarmupConfig> bullet_pool_warmups;
};
