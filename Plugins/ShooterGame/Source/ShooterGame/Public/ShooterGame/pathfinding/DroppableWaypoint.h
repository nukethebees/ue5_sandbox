// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "ShooterGame/interaction/Describable.h"
#include "SandboxGameShared/logging/LogMsgMixin.hpp"
#include "ShooterGame/logging/ShooterGameLogCategories.h"

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "DroppableWaypoint.generated.h"

class UStaticMeshComponent;
class URotatingActorComponent;

UCLASS()
class SHOOTERGAME_API ADroppableWaypoint
    : public AActor
    , public ml::LogMsgMixin<"ADroppableWaypoint", LogShooterGameActor>
    , public IDescribable {
    GENERATED_BODY()
  public:
    ADroppableWaypoint();

    void Activate();
    void Deactivate();

    // IDescribable
    virtual FText get_description() const override {
        static auto const desc{FText::FromName(TEXT("Waypoint"))};
        return desc;
    }
  protected:
    void BeginPlay() override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Waypoint")
    UStaticMeshComponent* mesh_component{nullptr};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Waypoint")
    URotatingActorComponent* rotation_component{nullptr};
};
