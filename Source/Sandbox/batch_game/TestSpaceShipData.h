#pragma once

#include <Sandbox/players/BarrelRoll.h>
#include <Sandbox/utilities/DrawDebugConfig.h>
#include <ShooterGame/players/SpeedResponse.h>

#include <SandboxCoreEngine/collision_settings.h>

#include <CoreMinimal.h>
#include <Engine/DataAsset.h>
#include <Engine/EngineTypes.h>

#include "TestSpaceShipData.generated.h"

class UStaticMesh;

class UTestTeamVisualData;

class AShipBomb;

UCLASS(BlueprintType)
class UTestSpaceShipData : public UDataAsset {
    GENERATED_BODY()
  public:
    /* ------------------------------------------------------------------------------------------ */
    // Visuals
    /* ------------------------------------------------------------------------------------------ */
    UPROPERTY(EditAnywhere, Category = "Visuals")
    float boost_effect_colour_intensity{75.f};

    UPROPERTY(EditAnywhere, Category = "Visuals")
    FLinearColor engine_colour{FLinearColor::Blue};

    UPROPERTY(EditAnywhere, Category = "Visuals")
    TObjectPtr<UTestTeamVisualData> team_visual_data{nullptr};

    /* ------------------------------------------------------------------------------------------ */
    // Energy
    /* ------------------------------------------------------------------------------------------ */
    UPROPERTY(VisibleAnywhere, Category = "Movement")
    float thrust_energy_max{1.f};

    /* ------------------------------------------------------------------------------------------ */
    // Movement
    /* ------------------------------------------------------------------------------------------ */
    UPROPERTY(EditAnywhere, Category = "Movement")
    FSpeedResponses speed_responses{};

    UPROPERTY(EditAnywhere, Category = "Movement")
    float cruise_speed{12000.0f};
    UPROPERTY(EditAnywhere, Category = "Movement")
    float thrust_recharge_time{7.f};

    UPROPERTY(EditAnywhere, Category = "Movement")
    float boost_depletion_time{4.f};
    UPROPERTY(EditAnywhere, Category = "Movement")
    float boost_speed{30000.0f};

    UPROPERTY(EditAnywhere, Category = "Movement")
    float brake_depletion_time{6.f};
    UPROPERTY(EditAnywhere, Category = "Movement")
    float brake_speed{1000.0f};

    UPROPERTY(EditAnywhere, Category = "Movement|Steering")
    float rotation_speed{60.f};

    UPROPERTY(EditAnywhere, Category = "Movement|Steering")
    float pitch_angle_max{30.f};
    UPROPERTY(EditAnywhere, Category = "Movement|Steering")
    float pitch_speed{3.f};

    UPROPERTY(EditAnywhere, Category = "Movement|Steering")
    float yaw_angle_max{30.f};
    UPROPERTY(EditAnywhere, Category = "Movement|Steering")
    float yaw_speed{3.f};

    UPROPERTY(EditAnywhere, Category = "Movement|Steering")
    float turn_bank_angle_max{30.f};
    UPROPERTY(EditAnywhere, Category = "Movement|Steering")
    float turn_bank_speed{2.f};

    UPROPERTY(EditAnywhere, Category = "Movement|Steering")
    float manual_bank_angle_max{90.f};
    UPROPERTY(EditAnywhere, Category = "Movement|Steering")
    float manual_bank_speed{5.f};

    UPROPERTY(EditAnywhere, Category = "Movement|Steering")
    FBarrelRollConfig barrel_roll_config;

    UPROPERTY(EditAnywhere, Category = "Movement")
    float auto_level_speed{10.f};

    UPROPERTY(EditAnywhere, Category = "Movement")
    float auto_level_roll_delay{1.f};

    UPROPERTY(EditAnywhere, Category = "Movement")
    float lateral_adjustment_speed{5000.f};
    UPROPERTY(EditAnywhere, Category = "Movement")
    float vertical_adjustment_speed{5000.f};

    /* ------------------------------------------------------------------------ */
    /* Combat */
    /* ------------------------------------------------------------------------ */
    UPROPERTY(EditAnywhere, Category = "Combat")
    float laser_firing_period{0.15f};

    UPROPERTY(EditAnywhere, Category = "Combat")
    float laser_lock_on_transition_delay{1.f};
    UPROPERTY(EditAnywhere, Category = "Combat")
    float laser_lock_on_distance{10000.f};

    UPROPERTY(EditAnywhere, Category = "Combat")
    int32 laser_damage{5};

    UPROPERTY(EditAnywhere, Category = "Combat")
    float laser_speed{10000.f};

    UPROPERTY(EditAnywhere, Category = "Combat")
    float laser_max_distance{10000.f};

    UPROPERTY(EditAnywhere, Category = "Combat")
    TSubclassOf<AShipBomb> bomb_class;
};
