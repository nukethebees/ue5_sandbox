#pragma once

#include "TestFlyBase.h"

#include "CoreMinimal.h"

#include "TestFlyChaseRotInterp.generated.h"

UCLASS()
class ATestFlyChaseRotInterp : public ATestFlyBase {
  public:
    GENERATED_BODY()

    ATestFlyChaseRotInterp();

    void Tick(float dt) override;
  protected:
    void BeginPlay() override;

    void move_to_target(float dt);

    UPROPERTY(EditAnywhere, Category = "Fly")
    TObjectPtr<AActor> target{nullptr};

    UPROPERTY(EditAnywhere, Category = "Fly")
    float speed{1000.f};
    UPROPERTY(EditAnywhere, Category = "Fly")
    float turn_speed_deg_per_s{60.f};
};
