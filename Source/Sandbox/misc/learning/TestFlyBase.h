#pragma once

#include "Sandbox/misc/learning/TestMaterialConfig.h"

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "TestFlyBase.generated.h"

class UBoxComponent;
class UStaticMeshComponent;

struct FHitResult;

UCLASS()
class ATestFlyBase : public AActor {
  public:
    GENERATED_BODY()

    ATestFlyBase();
  protected:
    void OnConstruction(FTransform const& transform) override;
    void BeginPlay() override;

    void configure_material();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fly")
    UStaticMeshComponent* main_mesh{nullptr};
    UPROPERTY(EditAnywhere, Category = "target")
    FTestMaterialConfig material_config;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fly")
    UBoxComponent* main_collision{nullptr};

    auto check_if_hit(FVector start, FVector end) -> FHitResult;
};
