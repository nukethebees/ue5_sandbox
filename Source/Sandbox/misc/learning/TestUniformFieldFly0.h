#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "TestUniformFieldFly0.generated.h"

class UStaticMeshComponent;
class ATestUniformField;

UCLASS()
class ATestUniformFieldFly0 : public AActor {
    GENERATED_BODY()
  public:
    ATestUniformFieldFly0();

    void Tick(float dt) override;
  protected:
    void BeginPlay() override;
    void OnConstruction(FTransform const& transform) override;

    UPROPERTY(EditAnywhere, Category = "Ship")
    TObjectPtr<UStaticMeshComponent> mesh{nullptr};
};
