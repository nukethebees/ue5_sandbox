// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "BenchmarkOrchestratorActor.generated.h"

class ACameraActor;

UCLASS()
class SANDBOX_API ABenchmarkOrchestratorActor : public AActor {
    GENERATED_BODY()
  public:
    ABenchmarkOrchestratorActor();
  protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(EEndPlayReason::Type const EndPlayReason) override;

    UPROPERTY(EditAnywhere, Category = "Benchmark")
    float benchmark_duration_seconds{10.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Benchmark")
    ACameraActor* benchmark_camera;
  private:
    void start_trace();
    void stop_trace();

    FTimerHandle trace_timer_handle;
};
