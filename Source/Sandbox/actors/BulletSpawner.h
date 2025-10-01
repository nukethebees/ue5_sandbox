// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Sandbox/mixins/log_msg_mixin.hpp"

#include "BulletSpawner.generated.h"

class ABulletActor;

UCLASS()
class SANDBOX_API ABulletSpawner
    : public AActor
    , public ml::LogMsgMixin<"ABulletSpawner"> {
    GENERATED_BODY()
  public:
    ABulletSpawner();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bullets")
    float bullets_per_second{1.0f};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bullets")
    float bullet_speed{5000.0f};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bullets")
    // TSubclassOf<AActor> bullet_class;
    TSubclassOf<ABulletActor> bullet_class;
  protected:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bullets")
    USceneComponent* fire_point{nullptr};
    virtual void BeginPlay() override;
  public:
    virtual void Tick(float DeltaTime) override;
  private:
    float time_since_last_shot{0.0f};
    void spawn_bullet();
};
