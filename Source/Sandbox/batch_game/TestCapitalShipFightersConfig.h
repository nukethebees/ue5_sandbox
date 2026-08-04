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
    float los_check_buffer{100.f};

    // Awareness
    UPROPERTY(EditAnywhere)
    float awareness_radius{50000.f};

    // Debugging
    UPROPERTY(EditAnywhere)
    FDrawDebugConfig debug_drawer;
};
