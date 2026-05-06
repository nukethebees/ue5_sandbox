#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "TestUniformFieldSink.generated.h"

class UStaticMeshComponent;
class ATestUniformField;

UCLASS()
class ATestUniformFieldPointSink : public AActor {
    GENERATED_BODY()
  public:
    ATestUniformFieldPointSink();

    void Tick(float dt) override;
  protected:
    void BeginPlay() override;
    void OnConstruction(FTransform const& transform) override;

    UPROPERTY(EditAnywhere, Category = "Sink")
    TObjectPtr<UStaticMeshComponent> point_mesh{nullptr};
    UPROPERTY(EditAnywhere, Category = "Sink")
    float base_speed{1000.f};
    UPROPERTY(EditAnywhere, Category = "Sink")
    float max_speed{2000.f};
    UPROPERTY(EditAnywhere, Category = "Sink")
    float max_acceleration{100.f};
    UPROPERTY(VisibleAnywhere, Category = "Sink")
    float speed{0.f};

    UPROPERTY(EditInstanceOnly, Category = "Sink")
    TObjectPtr<ATestUniformField> field{nullptr};
};
