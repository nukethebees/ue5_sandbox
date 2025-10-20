// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "Sandbox/health/data/HealthStationPayload.h"
#include "Sandbox/interaction/triggering/data/TriggerableId.h"

#include "HealthStationActor.generated.h"

class UWidgetComponent;
class UHealthStationWidget;
class USceneComponent;
struct FStationStateData;

UCLASS()
class SANDBOX_API AHealthStationActor : public AActor {
    GENERATED_BODY()
  public:
    AHealthStationActor();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health Station")
    FHealthStationPayload health_station_payload;

    UFUNCTION(BlueprintCallable, Category = "Health Station")
    float get_current_capacity() const { return health_station_payload.current_capacity; }

    UFUNCTION(BlueprintCallable, Category = "Health Station")
    bool is_ready() const { return health_station_payload.is_ready(); }
  protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(EEndPlayReason::Type reason) override;

    UPROPERTY(EditInstanceOnly, Category = "UI")
    USceneComponent* widget_pivot;

    UPROPERTY(VisibleAnywhere)
    UWidgetComponent* widget_component;

    UPROPERTY(EditDefaultsOnly, Category = "UI")
    TSubclassOf<UHealthStationWidget> health_station_widget_class;
  private:
    TriggerableId my_trigger_id{};

    UFUNCTION()
    void handle_station_state_changed(FStationStateData const& state_data);
};
