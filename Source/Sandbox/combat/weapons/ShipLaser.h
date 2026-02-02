#pragma once

#include "Sandbox/health/HealthChange.h"

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "ShipLaser.generated.h"

class UWorld;
class UBoxComponent;
class UProjectileMovementComponent;
class UStaticMeshComponent;
class UMaterialInterface;

class UShipLaserConfig;

UCLASS()
class SANDBOX_API AShipLaser : public AActor {
    GENERATED_BODY()
  public:
    AShipLaser();

    void Tick(float dt) override;

    void set_speed(float s) { speed = s; }
    auto get_speed(float s) const { return speed; }
    void set_config(UShipLaserConfig const& config);
  protected:
    void BeginPlay() override;
    void OnConstruction(FTransform const& transform);

    UFUNCTION()
    void on_hit(UPrimitiveComponent* HitComponent,
                AActor* other_actor,
                UPrimitiveComponent* other_component,
                FVector NormalImpulse,
                FHitResult const& Hit);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Laser")
    UBoxComponent* collision_component{nullptr};

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Laser")
    UStaticMeshComponent* mesh_component{nullptr};

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Laser")
    UMaterialInterface* material{nullptr};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Laser")
    FHealthChange damage{5.0f, EHealthChangeType::Damage};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Laser")
    float speed{10000.f};
};
