// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Sandbox/actor_components/HealthChange.h"

#include "HealthChangeOnOverlapComponent.generated.h"

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class SANDBOX_API UHealthChangeOnOverlapComponent : public UActorComponent {
    GENERATED_BODY()
  public:
    UHealthChangeOnOverlapComponent();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
    FHealthChange health_change;
  protected:
    virtual void BeginPlay() override;
  private:
    UFUNCTION()
    void on_overlap(AActor* OverlappedActor, AActor* OtherActor);
};
