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

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMaxSpeedChanged, float, NewMaxSpeed);

UENUM(BlueprintType)
enum class ECharacterCameraMode : uint8 {
    FirstPerson UMETA(DisplayName = "First Person"),
    ThirdPerson UMETA(DisplayName = "Third Person"),
    MAX UMETA(Hidden)
};

namespace ml {
inline static constexpr wchar_t MyCharacterLogTag[]{TEXT("MyCharacter")};
}

USTRUCT(BlueprintType)
struct FCharacterInputActions {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input)
    TObjectPtr<UInputMappingContext> first_person_context{};

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input)
    TObjectPtr<UInputAction> move_action{};

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input)
    TObjectPtr<UInputAction> jump_action{};

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input)
    TObjectPtr<UInputAction> jetpack_action{};

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input)
    TObjectPtr<UInputAction> cycle_camera_action{};
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

    // Torch
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Torch")
    USpotLightComponent* torch_component;

    // Abilities
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Abilities")
    UWarpComponent* warp_component{nullptr};
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

    // Camera
    UPROPERTY(VisibleAnywhere, Category = "Camera")
    UCameraComponent* first_person_camera_component{nullptr};
    UPROPERTY(VisibleAnywhere, Category = "Camera")
    UCameraComponent* third_person_camera_component{nullptr};
    UPROPERTY(VisibleAnywhere, Category = "Camera")
    USpringArmComponent* spring_arm{nullptr};
    UPROPERTY(VisibleAnywhere, Category = "Camera")
    ECharacterCameraMode camera_mode{ECharacterCameraMode::FirstPerson};

    // Movement
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement");
    float move_speed{800.0f};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement");
    float acceleration{100.0f};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement");
    UJetpackComponent* jetpack_component;
  private:
    virtual void handle_death();
    void disable_all_cameras();
    void change_camera_to(ECharacterCameraMode mode);

    int32 cached_jump_count{0};
    bool torch_on{true};
    APlayerController* player_controller{nullptr};
    AMyHUD* hud{nullptr};
};
