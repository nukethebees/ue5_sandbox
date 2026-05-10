#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "TestFlyOrbit3D.generated.h"

class UStaticMeshComponent;
class UArrowComponent;

UCLASS()
class ATestFlyOrbit3D : public AActor {
    GENERATED_BODY()
  public:
    ATestFlyOrbit3D();

    void Tick(float dt) override;
  protected:
    void BeginPlay() override;

    // Visuals
    UPROPERTY(EditAnywhere, Category = "Fly")
    TObjectPtr<UStaticMeshComponent> mesh{nullptr};
    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UArrowComponent> fwd_arrow;
    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UArrowComponent> right_arrow;

    UPROPERTY(EditAnywhere, Category = "Fly")
    float radius{1000.f};
    UPROPERTY(EditAnywhere, Category = "Fly")
    float rotation_speed_deg_per_s{45.f};
    UPROPERTY(VisibleAnywhere, Category = "Fly")
    float angle_rad{0.f};
    UPROPERTY(VisibleAnywhere, Category = "Fly")
    FVector origin;
    UPROPERTY(VisibleAnywhere, Category = "Fly")
    FVector u;
    UPROPERTY(VisibleAnywhere, Category = "Fly")
    FVector v;
};
