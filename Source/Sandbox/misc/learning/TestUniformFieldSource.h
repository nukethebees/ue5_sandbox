#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "TestUniformFieldSource.generated.h"

class UStaticMeshComponent;

USTRUCT()
struct FTestUniformFieldPointSourceData {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Field")
    FVector coordinate{};
    UPROPERTY(EditAnywhere, Category = "Field")
    float strength{0.f};
    UPROPERTY(EditAnywhere, Category = "Field")
    float falloff{-2.f}; // pow(distance, falloff)
    UPROPERTY(EditAnywhere, Category = "Field")
    FRotator rotation{FRotator::ZeroRotator};
};

UCLASS()
class ATestUniformFieldPointSource : public AActor {
    GENERATED_BODY()
  public:
    ATestUniformFieldPointSource();

    void Tick(float dt) override;
  protected:
    void BeginPlay() override;
    void OnConstruction(FTransform const& transform) override;

    UPROPERTY(EditAnywhere, Category = "Field")
    TObjectPtr<UStaticMeshComponent> point_mesh{nullptr};

    UPROPERTY(EditAnywhere, Category = "Field")
    FTestUniformFieldPointSourceData source{};
};
