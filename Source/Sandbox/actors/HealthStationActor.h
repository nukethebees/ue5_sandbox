// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HealthStationActor.generated.h"

class UHealthStationComponent;
class UWidgetComponent;
class UHealthStationWidget;
class USceneComponent;
struct FStationStateData;

UCLASS()
class SANDBOX_API AHealthStationActor : public AActor {
    GENERATED_BODY()
  public:
    AHealthStationActor();
  protected:
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere)
    UHealthStationComponent* health_station_component;

    UPROPERTY(EditInstanceOnly, Category = "UI")
    USceneComponent* widget_pivot;

    UPROPERTY(VisibleAnywhere)
    UWidgetComponent* widget_component;

    UPROPERTY(EditDefaultsOnly, Category = "UI")
    TSubclassOf<UHealthStationWidget> health_station_widget_class;

    UFUNCTION()
    void handle_station_state_changed(FStationStateData const& state_data);
};
