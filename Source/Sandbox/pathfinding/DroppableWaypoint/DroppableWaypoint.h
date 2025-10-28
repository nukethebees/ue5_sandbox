// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "Sandbox/logging/mixins/LogMsgMixin.hpp"
#include "Sandbox/logging/SandboxLogCategories.h"

#include "DroppableWaypoint.generated.h"

class UStaticMeshComponent;
class URotatingActorComponent;

UCLASS()
class SANDBOX_API ADroppableWaypoint
    : public AActor
    , public ml::LogMsgMixin<"ADroppableWaypoint", LogSandboxActor> {
    GENERATED_BODY()
  public:
    ADroppableWaypoint();

    void Activate();
    void Deactivate();
  protected:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Waypoint")
    UStaticMeshComponent* mesh_component{nullptr};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Waypoint")
    URotatingActorComponent* rotation_component{nullptr};
};
