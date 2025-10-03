// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MassArchetypeTypes.h"
#include "MassEntityTypes.h"

#include "Sandbox/mixins/log_msg_mixin.hpp"

#include "MassBulletSpawner.generated.h"

class ABulletActor;
class UArrowComponent;
class UBulletDataAsset;
class AMassBulletVisualizationActor;

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
  protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bullets")
    UArrowComponent* fire_point{nullptr};
  private:
    void spawn_bullet();
    FTransform get_spawn_transform(UArrowComponent const& bullet_start) const;

    float time_since_last_shot{0.0f};
};
