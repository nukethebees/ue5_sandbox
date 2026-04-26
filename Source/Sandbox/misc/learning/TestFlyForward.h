#pragma once

#include "TestFlyBase.h"

#include "CoreMinimal.h"

#include "TestFlyForward.generated.h"

UCLASS()
class ATestFlyForward : public ATestFlyBase {
  public:
    GENERATED_BODY()

    ATestFlyForward();

    void Tick(float dt) override;
  protected:
    void BeginPlay() override;

    UPROPERTY(EditAnywhere, Category = "Fly")
    float speed{1000.f};
};
