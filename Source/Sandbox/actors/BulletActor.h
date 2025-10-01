// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "NiagaraSystem.h"

#include "Sandbox/data/health/HealthChange.h"
#include "Sandbox/mixins/log_msg_mixin.hpp"

#include "BulletActor.generated.h"

UCLASS()
class SANDBOX_API ABulletActor
    : public AActor
    , public ml::LogMsgMixin<"ABulletActor"> {
    GENERATED_BODY()
  public:
    ABulletActor();
  protected:
    virtual void BeginPlay() override;

    UFUNCTION()
    void on_hit(UPrimitiveComponent* HitComponent,
                AActor* OtherActor,
                UPrimitiveComponent* OtherComponent,
                FVector NormalImpulse,
                FHitResult const& Hit);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bullet")
    UNiagaraSystem* impact_effect{nullptr};

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bullet")
    UBoxComponent* collision_component{nullptr};

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bullet")
    UStaticMeshComponent* mesh_component{nullptr};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
    FHealthChange damage{5.0f, EHealthChangeType::Damage};

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bullet")
    UProjectileMovementComponent* projectile_movement{nullptr};
};
