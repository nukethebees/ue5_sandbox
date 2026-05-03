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

    void find_field();

    UPROPERTY(EditAnywhere, Category = "Sink")
    TObjectPtr<UStaticMeshComponent> point_mesh{nullptr};
    UPROPERTY(EditAnywhere, Category = "Sink")
    float speed{1000.f};

    UPROPERTY()
    TWeakObjectPtr<ATestUniformField> field{nullptr};
};
