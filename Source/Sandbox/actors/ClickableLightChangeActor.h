// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include <array>

#include "Components/PointLightComponent.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Sandbox/interfaces/Clickable.h"

#include "ClickableLightChangeActor.generated.h"

UCLASS()
class SANDBOX_API AClickableLightChangeActor
    : public AActor
    , public IClickable {
    GENERATED_BODY()
  public:
    AClickableLightChangeActor();
  protected:
    virtual void BeginPlay() override;
  public:
    virtual void Tick(float DeltaTime) override;
    virtual void OnClicked() override;
  private:
    UPointLightComponent* point_light{nullptr};

    static inline std::array<FLinearColor, 4> colours{
        FLinearColor::Red, FLinearColor::Green, FLinearColor::Blue, FLinearColor::Yellow};
    int32 current_colour_index{0};
};
