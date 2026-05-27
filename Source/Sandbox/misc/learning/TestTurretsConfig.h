#pragma once

#include "CoreMinimal.h"
#include "CollisionShape.h"
#include "Engine/DataAsset.h"

#include "TestTurretsConfig.generated.h"

class UStaticMesh;

class AShipLaser;

UCLASS(BlueprintType)
class UTestTurretsConfig : public UDataAsset {
    GENERATED_BODY()
  public:
    UTestTurretsConfig();

    // Visuals
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TObjectPtr<UStaticMesh> body_mesh{nullptr};

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TObjectPtr<UStaticMesh> cannon_mesh{nullptr};

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FTransform body_offset{FTransform::Identity};

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FTransform cannon_offset{FTransform::Identity};

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FTransform yaw_pivot_offset{FTransform::Identity};

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FTransform pitch_pivot_offset{FTransform::Identity};

    // Collision
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FTransform collision_offset{FTransform::Identity};

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float collision_radius{100.f};

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float collision_half_height{100.f};

    // Laser
    UPROPERTY(VisibleAnywhere, Category = "Turret|Laser")
    TSubclassOf<AShipLaser> laser_class{nullptr};

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FTransform fire_point_offset{FTransform::Identity};

    // Rotation
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float yaw_speed_degrees{60.f};

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float pitch_speed_degrees{60.f};

    // Health
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 max_health{20};

    auto is_ready() const noexcept -> bool;
};
