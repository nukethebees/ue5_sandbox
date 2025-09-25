// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Sandbox/actor_components/RotatingActorComponent.h"
#include "Sandbox/data/health/HealthChange.h"
#include "Sandbox/mixins/log_msg_mixin.hpp"

#include "HealthPackActor.generated.h"

UCLASS()
class SANDBOX_API AHealthPackActor
    : public AActor
    , public ml::LogMsgMixin<"AHealthPackActor"> {
    GENERATED_BODY()
  public:
    AHealthPackActor();
  protected:
    virtual void BeginPlay() override;

    UFUNCTION()
    void on_overlap_begin(UPrimitiveComponent* OverlappedComponent,
                          AActor* OtherActor,
                          UPrimitiveComponent* OtherComponent,
                          int32 OtherBodyIndex,
                          bool bFromSweep,
                          FHitResult const& SweepResult);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Health Pack")
    UBoxComponent* collision_component{nullptr};

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Health Pack")
    UStaticMeshComponent* mesh_component{nullptr};

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Health Pack")
    URotatingActorComponent * rotating_component{};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
    FHealthChange healing{25.0f, EHealthChangeType::Healing};
};
