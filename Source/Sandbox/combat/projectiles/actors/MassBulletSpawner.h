// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MassArchetypeTypes.h"
#include "MassEntityTypes.h"

#include "Sandbox/mixins/LogMsgMixin.hpp"

#include "MassBulletSpawner.generated.h"

class ABulletActor;
class UArrowComponent;
class UBulletDataAsset;
class AMassBulletVisualizationActor;
class UMassBulletSubsystem;

UCLASS()
class SANDBOX_API AMassBulletSpawner
    : public AActor
    , public ml::LogMsgMixin<"AMassBulletSpawner"> {
    GENERATED_BODY()
  public:
    AMassBulletSpawner();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bullets")
    float bullets_per_second{1.0f};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bullets")
    float bullet_speed{5000.0f};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bullets")
    TObjectPtr<UBulletDataAsset> bullet_data{nullptr};
  protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bullets")
    UArrowComponent* fire_point_component_{nullptr};
  private:
    void spawn_bullet(UMassBulletSubsystem& mass_bullet_subsystem,
                      UArrowComponent const& fire_point,
                      float travel_time);
    FTransform get_spawn_transform(UArrowComponent const& bullet_start, float travel_time) const;

    float time_since_last_shot{0.0f};
};
