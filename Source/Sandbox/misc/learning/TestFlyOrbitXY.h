#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "TestFlyOrbitXY.generated.h"

class UStaticMeshComponent;

UCLASS()
class ATestFlyOrbitXY : public AActor {
    GENERATED_BODY()
  public:
    ATestFlyOrbitXY();

    void Tick(float dt) override;
  protected:
    void BeginPlay() override;

    // Visuals
    UPROPERTY(EditAnywhere, Category = "Fly")
    TObjectPtr<UStaticMeshComponent> mesh{nullptr};

    UPROPERTY(EditAnywhere, Category = "Fly")
    float radius{1000.f};
    UPROPERTY(EditAnywhere, Category = "Fly")
    float rotation_speed_deg_per_s{45.f};
    UPROPERTY(VisibleAnywhere, Category = "Fly")
    float angle_rad{0.f};
    UPROPERTY(VisibleAnywhere, Category = "Fly")
    FVector origin;
};
