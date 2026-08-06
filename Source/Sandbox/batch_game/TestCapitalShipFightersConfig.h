#pragma once

#include "Sandbox/utilities/DrawDebugConfig.h"

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

#include "TestCapitalShipFightersConfig.generated.h"

class UStaticMesh;

class UTestTeamVisualData;

UCLASS(BlueprintType)
class UTestCapitalShipFightersConfig : public UDataAsset {
    GENERATED_BODY()
  public:
    UTestCapitalShipFightersConfig() = default;

    // Visuals
    UPROPERTY(EditAnywhere)
    TObjectPtr<UStaticMesh> mesh{nullptr};

    UPROPERTY(EditAnywhere, Category = "Visuals")
    TObjectPtr<UTestTeamVisualData> team_visual_data{nullptr};

    // Movement
    UPROPERTY(EditAnywhere)
    float speed{2000.f};

    UPROPERTY(EditAnywhere)
    float turn_speed_unitless{1.f};

    // Combat
    UPROPERTY(EditAnywhere)
    int32 laser_damage{10};

    UPROPERTY(EditAnywhere)
    float laser_speed{10000.f};

    UPROPERTY(EditAnywhere)
    float laser_max_distance{10000.f};

    UPROPERTY(EditAnywhere)
    int32 health{50};

    UPROPERTY(EditAnywhere)
    float fire_cooldown{0.33f};

    UPROPERTY(EditAnywhere)
    float attack_retry_cooldown{0.15f};

    UPROPERTY(EditAnywhere)
    float attack_engagement_threshold{5000.f};

    UPROPERTY(EditAnywhere,
              Category = "Combat",
              meta = (ClampMin = "0.001", UIMin = "0.001", Units = "Hz"))
    float attack_reposition_frequency{10.f};

    UPROPERTY(EditAnywhere,
              Category = "Combat",
              meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
    float desired_attack_distance_ratio{0.5f};

    UPROPERTY(EditAnywhere,
              Category = "Combat",
              meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
    float inner_attack_distance_ratio{0.4f};

    UPROPERTY(EditAnywhere,
              Category = "Combat",
              meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
    float outer_attack_distance_ratio{0.6f};

    UPROPERTY(EditAnywhere,
              Category = "Combat",
              meta = (ClampMin = "0.0", UIMin = "0.0", Units = "cm"))
    float arrival_distance{500.f};

    UPROPERTY(EditAnywhere)
    float los_check_buffer{100.f};

    // Awareness
    UPROPERTY(EditAnywhere)
    float awareness_radius{10000.f};

    UPROPERTY(EditAnywhere,
              Category = "Awareness",
              meta = (ClampMin = "0.001", UIMin = "0.001", Units = "Hz"))
    float awareness_scan_frequency{6.f};

    // What dot product we need to deviate off course and fight someone nearby
    UPROPERTY(EditAnywhere)
    float minimum_opportunistic_intercept_deviation_dot_product{0.5f};

    // Debugging
    UPROPERTY(EditAnywhere)
    FDrawDebugConfig debug_drawer;
};
