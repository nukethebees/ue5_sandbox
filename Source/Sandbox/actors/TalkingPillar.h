// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Sandbox/mixins/print_msg_mixin.hpp"
#include "Sandbox/interfaces/Clickable.h"

#include "TalkingPillar.generated.h"

UCLASS()
class SANDBOX_API ATalkingPillar
    : public AActor
    , public print_msg_mixin
    , public IClickable {
    GENERATED_BODY()
  public:
    // Sets default values for this actor's properties
    ATalkingPillar();
  protected:
    // Called when the game starts or when spawned
    virtual void BeginPlay() override;
  public:
    // Called every frame
    virtual void Tick(float DeltaTime) override;

    virtual void OnClicked() override { print_msg("Ahhhhh!"); }

    UPROPERTY(VisibleAnywhere)
    UStaticMeshComponent* CubeMesh;
};
