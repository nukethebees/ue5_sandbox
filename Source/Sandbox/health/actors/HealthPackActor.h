// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Actor.h"
#include "Sandbox/environment/effects/actor_components/RotatingActorComponent.h"
#include "Sandbox/health/data/HealthChange.h"
#include "Sandbox/logging/mixins/LogMsgMixin.hpp"

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
    void on_overlap_begin(UPrimitiveComponent* overlapped_component,
                          AActor* other_actor,
                          UPrimitiveComponent* other_component,
                          int32 other_body_index,
                          bool from_sweep,
                          FHitResult const& sweep_result);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Health Pack")
    UBoxComponent* collision_component{nullptr};

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Health Pack")
    UStaticMeshComponent* mesh_component{nullptr};

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Health Pack")
    URotatingActorComponent* rotating_component{};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
    FHealthChange healing{25.0f, EHealthChangeType::Healing};
};
