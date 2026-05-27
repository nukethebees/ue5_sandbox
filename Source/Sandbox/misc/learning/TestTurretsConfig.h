#pragma once

#include "Sandbox/utilities/DrawDebugConfig.h"

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

    auto is_ready() const noexcept -> bool;

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

    // Vision
    UPROPERTY(VisibleAnywhere, Category = "Turret")
    float detection_radius{4000.f};

    // Collision
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FTransform collision_offset{FTransform::Identity};

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float collision_radius{100.f};

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float collision_half_height{100.f};

    // Laser
    UPROPERTY(EditAnywhere)
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

    UPROPERTY(EditAnywhere)
    FDrawDebugConfig searching_debug_drawer;

    UPROPERTY(EditAnywhere)
    FDrawDebugConfig attacking_debug_drawer;

    // Debugging
    UPROPERTY(EditAnywhere)
    FVector3f debug_sphere_offset{FVector3f::ZeroVector};
};
