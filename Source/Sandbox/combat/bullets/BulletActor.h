// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "Sandbox/health/data/HealthChange.h"
#include "Sandbox/logging/mixins/LogMsgMixin.hpp"
#include "Sandbox/logging/SandboxLogCategories.h"

#include "BulletActor.generated.h"

class UWorld;
class UBoxComponent;
class UNiagaraSystem;
class UProjectileMovementComponent;
class UStaticMeshComponent;

UCLASS()
class SANDBOX_API ABulletActor
    : public AActor
    , public ml::LogMsgMixin<"ABulletActor", LogSandboxActor> {
    GENERATED_BODY()
  public:
    ABulletActor();

    static auto fire(UWorld& world,
                     TSubclassOf<ABulletActor> bullet_class,
                     FVector spawn_location,
                     FRotator spawn_rotation,
                     float speed,
                     FHealthChange damage) -> ABulletActor*;

    void Activate();
    void Deactivate();
  protected:
    virtual void BeginPlay() override;

    UFUNCTION()
    void on_hit(UPrimitiveComponent* HitComponent,
                AActor* other_actor,
                UPrimitiveComponent* other_component,
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
