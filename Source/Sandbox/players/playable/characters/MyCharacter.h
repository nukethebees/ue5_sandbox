// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include <type_traits>
#include <utility>

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GenericTeamAgentInterface.h"
#include "InputActionValue.h"

#include "Sandbox/health/DeathHandler.h"
#include "Sandbox/input/mixins/EnhancedInputMixin.hpp"
#include "Sandbox/logging/LogMsgMixin.hpp"
#include "Sandbox/logging/SandboxLogCategories.h"
#include "Sandbox/players/common/enums/TeamID.h"
#include "Sandbox/players/playable/data/CameraConfig.h"
#include "Sandbox/players/playable/data/HumanoidMovement.h"
#include "Sandbox/players/playable/data/PlayerSkills.h"
#include "Sandbox/players/playable/delegates/MyCharacterDelegates.h"
#include "Sandbox/players/playable/interfaces/MaxSpeedChangeListener.h"
#include "Sandbox/players/playable/interfaces/MovementMultiplierReceiver.h"
#include "Sandbox/utilities/enums.h"

#include "MyCharacter.generated.h"

class UInputMappingContext;
class UInputAction;
class UInputComponent;
class USpotLightComponent;
class UCameraComponent;

class AMyPlayerController;
class AMyHUD;

class UArrowComponent;
class USpringArmComponent;
class UCharacterMovementComponent;

class UInteractorComponent;
class USpeedBoostComponent;
class UJetpackComponent;
class UHealthComponent;
class UCoinCollectorActorComponent;
class UPawnWeaponComponent;
class UInventoryComponent;
class UWarpComponent;
class UActorDescriptionScannerComponent;

class ADroppableWaypoint;

namespace ml {
namespace AMyCharacter {
inline static constexpr int32 camera_count{static_cast<int32>(ECharacterCameraMode::MAX)};
inline static constexpr FCameraConfig camera_configs[camera_count] = {
    {ECharacterCameraMode::FirstPerson, "Camera", false, true},
    {ECharacterCameraMode::ThirdPerson, "ThirdPersonCamera", true, true}};

consteval int32 count_required_spring_arms() {
    int32 count{0};
    for (auto const& config : camera_configs) {
        if (config.needs_spring_arm) {
            ++count;
        }
    }
    return count;
}
}
}

UCLASS()
class SANDBOX_API AMyCharacter
    : public ACharacter
    , public ml::LogMsgMixin<"MyCharacter", LogSandboxCharacter>
    , public IDeathHandler
    , public IMaxSpeedChangeListener
    , public IMovementMultiplierReceiver
    , public ml::EnhancedInputMixin
    , public IGenericTeamAgentInterface {
    GENERATED_BODY()
  public:
    static constexpr int32 default_max_jump_count{2};

    AMyCharacter();

    // Camera
    UCameraComponent const* get_active_camera() const;

    // Input Binding
    // -----------------------
    // Movement
    UFUNCTION()
    void move(FInputActionValue const& value);
    UFUNCTION()
    void start_jetpack();
    UFUNCTION()
    void stop_jetpack();
    UFUNCTION()
    void start_crouch();
    UFUNCTION()
    void stop_crouch();
    UFUNCTION()
    void start_sprint();
    UFUNCTION()
    void stop_sprint();

    // Vision
    UFUNCTION()
    void cycle_camera();
    UFUNCTION()
    void look(FInputActionValue const& value);

    // Combat
    UFUNCTION()
    void attack_started();
    UFUNCTION()
    void attack_continued(float delta_time);
    UFUNCTION()
    void attack_ended();

    // Inventory
    UFUNCTION()
    void cycle_next_weapon();
    UFUNCTION()
    void cycle_prev_weapon();
    UFUNCTION()
    void unequip_weapon();
    UFUNCTION()
    void drop_weapon();
    UFUNCTION()
    void reload_weapon();

    // Levels
    int32 upgrade_cost(int32 level) { return level * 10; }

    // Torch
    UFUNCTION()
    void aim_torch(FVector const& world_location);
    UFUNCTION()
    void reset_torch_position();
    UFUNCTION()
    void toggle_torch();
    UFUNCTION()
    void set_torch(bool on);
    UFUNCTION()
    void scroll_torch_cone(FInputActionValue const& value);

    // Jumping
    UFUNCTION()
    void reset_max_jump_count();
    UFUNCTION()
    void increase_max_jump_count(int32 jumps);
    virtual void OnJumped_Implementation() override;
    virtual void Landed(FHitResult const& Hit) override;
    void broadcast_jump_count() const;
    void broadcast_jump_count(int32 count) const;

    // Interaction
    UFUNCTION()
    void interact(FVector sweep_origin, FRotator sweep_direction);
    UFUNCTION()
    void drop_waypoint();

    // IGenericTeamAgentInterface
    virtual FGenericTeamId GetGenericTeamId() const override;
    virtual void SetGenericTeamId(FGenericTeamId const& TeamID) override;

    // Properties
    UPROPERTY()
    bool is_forced_movement{false};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Abilities")
    FPlayerSkills skills;

    FOnMaxSpeedChanged on_max_speed_changed;
    FOnJumpCountChanged on_jump_count_changed;

    // Components
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    UCoinCollectorActorComponent* coins{nullptr};

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    UInteractorComponent* interactor{nullptr};

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    UHealthComponent* health{nullptr};

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    USpeedBoostComponent* speed_boost{nullptr};

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movement")
    UJetpackComponent* jetpack{nullptr};

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Torch")
    USpotLightComponent* torch{nullptr};

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Abilities")
    UWarpComponent* warp{nullptr};

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapons")
    UPawnWeaponComponent* weapon_component{nullptr};

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
    UInventoryComponent* inventory{nullptr};

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
    UArrowComponent* weapon_attach_point{nullptr};

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI")
    UActorDescriptionScannerComponent* actor_description_scanner{nullptr};

    // Camera components
    static constexpr int32 camera_count{ml::AMyCharacter::camera_count};
    static constexpr int32 spring_arm_count{ml::AMyCharacter::count_required_spring_arms()};

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
    TArray<UCameraComponent*> cameras{};
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
    TArray<USpringArmComponent*> spring_arms{};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
    ECharacterCameraMode camera_mode{ECharacterCameraMode::FirstPerson};

    // Bullets
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bullets")
    TSubclassOf<ADroppableWaypoint> waypoint_class;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bullets")
    TArray<ADroppableWaypoint*> waypoints;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Team")
    ETeamID team_id{ETeamID::Player};
  protected:
    virtual void BeginPlay() override;
    virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

    // Movement
    virtual void on_speed_changed(float new_speed);
    virtual void set_movement_multiplier(float multiplier) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement");
    FHumanoidMovement movement{};
  private:
    // IDeathHandler
    virtual void handle_death() override;

    // Cameras
    void disable_all_cameras();
    void change_camera_to(ECharacterCameraMode mode);

    // Movement
    void set_speed(float new_speed);
    void update_speed();

    bool torch_on{true};
    APlayerController* player_controller{nullptr};
};
