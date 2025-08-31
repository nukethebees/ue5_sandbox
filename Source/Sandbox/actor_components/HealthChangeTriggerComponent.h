// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Sandbox/actor_components/HealthChange.h"

#include "HealthChangeTriggerComponent.generated.h"

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class SANDBOX_API UHealthChangeTriggerComponent : public UActorComponent {
    GENERATED_BODY()
  public:
    UHealthChangeTriggerComponent();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
    FHealthChange health_change;
  protected:
    virtual void BeginPlay() override;
  public:
    UFUNCTION(BlueprintCallable, Category = "Health")
    void change_health(AActor* OverlappedActor, AActor* OtherActor);
};
