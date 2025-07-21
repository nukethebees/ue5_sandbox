// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Clickable.h"

#include "RotatingOnClickActor.generated.h"

UCLASS()
class SANDBOX_API ARotatingOnClickActor
    : public AActor
    , public IClickable {
    GENERATED_BODY()
  public:
    ARotatingOnClickActor();
  protected:
    virtual void BeginPlay() override;
  public:
    virtual void Tick(float DeltaTime) override;
    virtual void OnClicked() override;
  private:
    bool is_rotating{false};
    float rotation_timer{0};
};
