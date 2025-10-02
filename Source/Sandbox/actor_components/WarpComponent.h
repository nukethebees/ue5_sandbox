// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "WarpComponent.generated.h"

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class SANDBOX_API UWarpComponent : public UActorComponent {
    GENERATED_BODY()
  public:
    UWarpComponent();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Warp")
    float max_warp_distance = 2000.0f;

    void warp_to_location(AActor* parent, FVector const& target_location);
};
