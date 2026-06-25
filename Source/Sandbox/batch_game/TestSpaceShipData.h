#pragma once

#include <Sandbox/players/BarrelRoll.h>
#include <Sandbox/utilities/DrawDebugConfig.h>

#include <SandboxCore/collision_settings.h>

#include <CoreMinimal.h>
#include <Engine/DataAsset.h>
#include <Engine/EngineTypes.h>

#include "TestSpaceShipData.generated.h"

class UStaticMesh;

class AShipLaser;

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

    /* ------------------------------------------------------------------------------------------ */
    // Energy
    /* ------------------------------------------------------------------------------------------ */
    UPROPERTY(VisibleAnywhere, Category = "Energy")
    float thrust_energy_max{1.f};

    /* ------------------------------------------------------------------------------------------ */
    // Movement
    /* ------------------------------------------------------------------------------------------ */
    UPROPERTY(EditAnywhere, Category = "Speed")
    float cruise_speed{12000.0f};
    UPROPERTY(EditAnywhere, Category = "Speed")
    float thrust_recharge_time{7.f};

    UPROPERTY(EditAnywhere, Category = "Speed")
    float boost_depletion_time{4.f};
    UPROPERTY(EditAnywhere, Category = "Speed")
    float boost_speed{30000.0f};

    UPROPERTY(EditAnywhere, Category = "Speed")
    float brake_depletion_time{6.f};
    UPROPERTY(EditAnywhere, Category = "Speed")
    float brake_speed{1000.0f};

    UPROPERTY(EditAnywhere, Category = "Steering")
    float rotation_speed{60.f};

    UPROPERTY(EditAnywhere, Category = "Steering|Pitch")
    float pitch_angle_max{30.f};
    UPROPERTY(EditAnywhere, Category = "Steering|Pitch")
    float pitch_speed{3.f};

    UPROPERTY(EditAnywhere, Category = "Steering|Yaw")
    float yaw_angle_max{30.f};
    UPROPERTY(EditAnywhere, Category = "Steering|Yaw")
    float yaw_speed{3.f};

    UPROPERTY(EditAnywhere, Category = "Steering|Roll")
    float turn_bank_angle_max{30.f};
    UPROPERTY(EditAnywhere, Category = "Steering|Roll")
    float turn_bank_speed{2.f};

    UPROPERTY(EditAnywhere, Category = "Steering|Roll")
    float manual_bank_angle_max{90.f};
    UPROPERTY(EditAnywhere, Category = "Steering|Roll")
    float manual_bank_speed{5.f};

    UPROPERTY(EditAnywhere, Category = "Steering|Roll")
    FBarrelRollConfig barrel_roll_config;

    UPROPERTY(EditAnywhere, Category = "Speed")
    float auto_level_speed{10.f};

    /* ------------------------------------------------------------------------ */
    /* Combat */
    /* ------------------------------------------------------------------------ */
    UPROPERTY(EditAnywhere, Category = "Laser")
    float laser_speed{1000.f};
    UPROPERTY(EditAnywhere, Category = "Laser")
    float laser_firing_period{0.15f};

    UPROPERTY(EditAnywhere, Category = "Laser")
    int32 lasers_per_burst{3};

    UPROPERTY(EditAnywhere, Category = "Laser")
    float laser_lock_on_transition_delay{1.f};
    UPROPERTY(EditAnywhere, Category = "Laser")
    float laser_lock_on_distance{10000.f};

    UPROPERTY(EditAnywhere, Category = "Laser")
    UShipLaserConfig* laser_config;
    UPROPERTY(EditAnywhere, Category = "Laser")
    UShipLaserConfig* hyper_laser_config;

    UPROPERTY(EditAnywhere, Category = "Bomb")
    TSubclassOf<AShipBomb> bomb_class;
};
