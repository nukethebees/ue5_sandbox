// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Sandbox/interfaces/Clickable.h"

#include "MovesWhenClickedActor.generated.h"

UCLASS()
class SANDBOX_API AMovesWhenClickedActor
    : public AActor
    , public IClickable {
    GENERATED_BODY()
  public:
    AMovesWhenClickedActor();
  protected:
    // Called when the game starts or when spawned
    virtual void BeginPlay() override;
  public:
    // Called every frame
    virtual void Tick(float DeltaTime) override;
    virtual void OnClicked() override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
    float move_speed{300.0f};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
    float acceleration{100.0f};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
    float move_duration{2.0f};
  private:
    bool is_moving{false};
    float move_timer{0};
};
