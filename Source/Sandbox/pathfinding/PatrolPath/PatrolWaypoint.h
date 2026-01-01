// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "PatrolWaypoint.generated.h"

class UStaticMeshComponent;

UCLASS()
class SANDBOX_API APatrolWaypoint : public AActor {
    GENERATED_BODY()
  public:
    APatrolWaypoint();

    auto get_point() const -> FVector { return this->GetActorLocation(); }

#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& event) override;
    static void OnActorLabelChanged(AActor* actor);
#endif
  protected:
    void OnConstruction(FTransform const& Transform);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Navigation")
    FName name;

#if WITH_EDITORONLY_DATA
    UPROPERTY(EditAnywhere, Category = "Navigation")
    UStaticMeshComponent* editor_mesh{nullptr};
#endif
};
