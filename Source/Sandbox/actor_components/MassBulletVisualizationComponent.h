// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Components/ActorComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "CoreMinimal.h"

#include "MassBulletVisualizationComponent.generated.h"

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class SANDBOX_API UMassBulletVisualizationComponent : public UActorComponent {
    GENERATED_BODY()
  public:
    UMassBulletVisualizationComponent();

    int32 add_instance(FTransform const& transform);
    void update_instance(int32 instance_index, FTransform const& transform);
    void remove_instance(int32 instance_index);

    UInstancedStaticMeshComponent* get_ismc() const { return ismc; }
  protected:
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mass Bullet")
    UInstancedStaticMeshComponent* ismc{nullptr};

    TArray<int32> free_indices{};
};
