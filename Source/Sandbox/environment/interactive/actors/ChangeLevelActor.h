// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "ChangeLevelActor.generated.h"

class UBoxComponent;

UCLASS()
class SANDBOX_API AChangeLevelActor : public AActor {
    GENERATED_BODY()
  public:
    AChangeLevelActor();
  protected:
    virtual void BeginPlay() override;

    UFUNCTION()
    void on_overlap_begin(UPrimitiveComponent* overlapped_component,
                          AActor* other_actor,
                          UPrimitiveComponent* other_component,
                          int32 other_body_index,
                          bool from_sweep,
                          FHitResult const& sweep_result);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level")
    TSoftObjectPtr<UWorld> level_to_load;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Components")
    UBoxComponent* collision_box;
};
