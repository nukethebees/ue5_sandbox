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
#include "Sandbox/mixins/log_msg_mixin.hpp"
#include "Sandbox/mixins/print_msg_mixin.hpp"
#include "Sandbox/utilities/enums.h"

#include "MyCharacter.generated.h"

class UInputMappingContext;
class UInputAction;
class UInputComponent;
class AMyPlayerController;
class UJetpackComponent;
class AMyHUD;

class USpeedBoostComponent;
class UInteractorComponent;
class USpeedBoostComponent;
class UJetpackComponent;
class UHealthComponent;
class UInteractorComponent;
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
inline static constexpr wchar_t MyCharacterLogTag[]{TEXT("MyCharacter")};

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
    TObjectPtr<UInputMappingContext> first_person_context{nullptr};

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input)
    TObjectPtr<UInputAction> move_action{nullptr};

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input)
    TObjectPtr<UInputAction> jump_action{nullptr};

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input)
    TObjectPtr<UInputAction> jetpack_action{nullptr};

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input)
    TObjectPtr<UInputAction> cycle_camera_action{nullptr};
};

UCLASS()
class SANDBOX_API AMyCharacter
    : public ACharacter
    , public print_msg_mixin
    , public ml::LogMsgMixin<ml::MyCharacterLogTag>
    , public IDeathHandler
    , public IMaxSpeedChangeListener {
    GENERATED_BODY()
  public:
    static constexpr int32 default_max_jump_count{2};

    AMyCharacter();

    UFUNCTION()
    void set_player_controller(AMyPlayerController* pc) {
        if (pc) {
            my_player_controller = pc;
        }
    }

    // Camera
    UCameraComponent const* get_active_camera() const;

    // Input
    UFUNCTION()
    void move(FInputActionValue const& value);
    UFUNCTION()
    void look(FInputActionValue const& value);
    UFUNCTION()
    void start_jetpack(FInputActionValue const& value);
    UFUNCTION()
    void stop_jetpack(FInputActionValue const& value);
    UFUNCTION()
    void cycle_camera(FInputActionValue const& value);

    // Torch
    UFUNCTION()
    void aim_torch(FVector const& world_location);
    UFUNCTION()
    void reset_torch_position();
    UFUNCTION()
    void toggle_torch();
    UFUNCTION()
    void set_torch(bool on);

    // Jumping
    UFUNCTION()
    void reset_max_jump_count();
    UFUNCTION()
    void increase_max_jump_count(int32 jumps);

    UPROPERTY()
    bool is_forced_movement{false};

    UPROPERTY(BlueprintAssignable, Category = "Movement")
    FOnMaxSpeedChanged OnMaxSpeedChanged;

    // Components
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    UCoinCollectorActorComponent* coins{nullptr};

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    UInteractorComponent* interactor{nullptr};

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    UHealthComponent* health{nullptr};

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    USpeedBoostComponent* speed_boost{nullptr};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
    UJetpackComponent* jetpack{nullptr};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Torch")
    USpotLightComponent* torch{nullptr};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Abilities")
    UWarpComponent* warp{nullptr};

    // Camera components
    static constexpr int32 camera_count{ml::AMyCharacter::camera_count};
    static constexpr int32 spring_arm_count{ml::AMyCharacter::count_required_spring_arms()};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
    TArray<UCameraComponent*> cameras{};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
    TArray<USpringArmComponent*> spring_arms{};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
    ECharacterCameraMode camera_mode{ECharacterCameraMode::FirstPerson};
  protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;
    virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

    // Speed
    virtual void on_speed_changed(float new_speed);

    // Input
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input)
    FCharacterInputActions input_actions{};

    UPROPERTY()
    AMyPlayerController* my_player_controller;

    // Movement
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement");
    float move_speed{800.0f};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement");
    float acceleration{100.0f};
  private:
    virtual void handle_death();
    void disable_all_cameras();
    void change_camera_to(ECharacterCameraMode mode);

    int32 cached_jump_count{0};
    bool torch_on{true};
    APlayerController* player_controller{nullptr};
    AMyHUD* hud{nullptr};
};
