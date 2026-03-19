// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/TriggerBox.h"

#include "TestLightShaft.generated.h"

class UStaticMeshComponent;
class UMaterialInstance;
class UMaterialInstanceDynamic;

UCLASS()
class SANDBOX_API ATestLightShaft : public AActor {
    GENERATED_BODY()
  public:
    ATestLightShaft();
  protected:
    void BeginPlay() override;
    void OnConstruction(FTransform const& transform) override;

    UPROPERTY(EditAnywhere, Category = "Light")
    UStaticMeshComponent* light_shaft_mesh{nullptr};
    UPROPERTY(EditAnywhere, Category = "Light")
    UMaterialInstance* light_material_base{nullptr};
    UPROPERTY(VisibleAnywhere, Category = "Light")
    UMaterialInstanceDynamic* light_material_instance{nullptr};
};
