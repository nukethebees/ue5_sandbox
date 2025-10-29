// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "Sandbox/interaction/interfaces/Describable.h"
#include "Sandbox/logging/mixins/LogMsgMixin.hpp"
#include "Sandbox/logging/SandboxLogCategories.h"

#include "DroppableWaypoint.generated.h"

class UStaticMeshComponent;
class URotatingActorComponent;

UCLASS()
class SANDBOX_API ADroppableWaypoint
    : public AActor
    , public ml::LogMsgMixin<"ADroppableWaypoint", LogSandboxActor>
    , public IDescribable {
    GENERATED_BODY()
  public:
    ADroppableWaypoint();

    void Activate();
    void Deactivate();

    // IDescribable
    virtual FText const& get_description() const override {
        static auto const desc{FText::FromName(TEXT("Waypoint"))};
        return desc;
    }
  protected:
    virtual void BeginPlay() override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Waypoint")
    UStaticMeshComponent* mesh_component{nullptr};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Waypoint")
    URotatingActorComponent* rotation_component{nullptr};
};
