// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include <type_traits>
#include <utility>

#include "Blueprint/UserWidget.h"
#include "Camera/CameraComponent.h"
#include "Components/SpotLightComponent.h"
#include "CoreMinimal.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputActionValue.h"
#include "Sandbox/actor_components/WarpComponent.h"
#include "Sandbox/interfaces/DeathHandler.h"
#include "Sandbox/interfaces/MaxSpeedChangeListener.h"
#include "Sandbox/mixins/EnhancedInputMixin.hpp"
#include "Sandbox/mixins/log_msg_mixin.hpp"
#include "Sandbox/mixins/print_msg_mixin.hpp"
#include "Sandbox/utilities/enums.h"

#include "MyCharacter.generated.h"

class UInputMappingContext;
class UInputAction;
class UInputComponent;

class AMyPlayerController;
class AMyHUD;

class UInteractorComponent;
class USpeedBoostComponent;
class UJetpackComponent;
class UHealthComponent;
class UCoinCollectorActorComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMaxSpeedChanged, float, NewMaxSpeed);

UENUM(BlueprintType)
enum class ECharacterCameraMode : uint8 {
    FirstPerson UMETA(DisplayName = "First Person"),
    ThirdPerson UMETA(DisplayName = "Third Person"),
    MAX UMETA(Hidden)
};

struct FCameraConfig {
    constexpr FCameraConfig(ECharacterCameraMode camera_mode,
                            char const* component_name,
                            bool needs_spring_arm,
                            bool use_pawn_control_rotation)
        : camera_mode(camera_mode)
        , camera_index(std::to_underlying(camera_mode))
        , component_name(component_name)
        , needs_spring_arm(needs_spring_arm)
        , use_pawn_control_rotation(use_pawn_control_rotation) {}

    ECharacterCameraMode camera_mode;
    std::underlying_type_t<ECharacterCameraMode> camera_index;
    char const* component_name;
    bool needs_spring_arm;
    bool use_pawn_control_rotation;
};

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

USTRUCT(BlueprintType)
struct FCharacterInputActions {
    GENERATED_BODY()

    FCharacterInputActions() = default;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input)
    UInputMappingContext* character_context{nullptr};

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input)
    UInputAction* move{nullptr};
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input)
    UInputAction* jump{nullptr};
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input)
    UInputAction* jetpack{nullptr};
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input)
    UInputAction* cycle_camera{nullptr};
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    UInputAction* toggle_torch{nullptr};
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    UInputAction* scroll_torch_cone{nullptr};
};

UCLASS()
class SANDBOX_API AMyCharacter
    : public ACharacter
    , public print_msg_mixin
    , public ml::LogMsgMixin<"MyCharacter">
    , public IDeathHandler
    , public IMaxSpeedChangeListener
    , public ml::EnhancedInputMixin {
    GENERATED_BODY()
  public:
    static constexpr int32 default_max_jump_count{2};

    AMyCharacter();

    // Camera
    UCameraComponent const* get_active_camera() const;

    // Input
    UFUNCTION()
    void move(FInputActionValue const& value);
    UFUNCTION()
    void look(FInputActionValue const& value);
    UFUNCTION()
    void start_jetpack();
    UFUNCTION()
    void stop_jetpack();
    UFUNCTION()
    void cycle_camera();
    UFUNCTION()
    void attack();

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

    UPROPERTY()
    bool is_forced_movement{false};

    UPROPERTY(BlueprintAssignable, Category = "Movement")
    FOnMaxSpeedChanged OnMaxSpeedChanged;

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
    TSubclassOf<AActor> bullet_class;
  protected:
    virtual void BeginPlay() override;
    virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

    // Speed
    virtual void on_speed_changed(float new_speed);

    // Input
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input)
    FCharacterInputActions input_actions{};

    // Movement
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement");
    float move_speed{800.0f};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement");
    float acceleration{100.0f};
  private:
    virtual void handle_death();
    void disable_all_cameras();
    void change_camera_to(ECharacterCameraMode mode);

    bool torch_on{true};
    APlayerController* player_controller{nullptr};
    AMyHUD* hud{nullptr};
};
