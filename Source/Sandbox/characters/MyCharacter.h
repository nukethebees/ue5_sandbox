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
#include "Sandbox/mixins/print_msg_mixin.hpp"
#include "Sandbox/mixins/log_msg_mixin.hpp"

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

template <typename Enum>
concept HasMaxEnumValue = requires { Enum::MAX; };

template <typename Enum, auto MAX_VALUE = Enum::MAX>
Enum get_next(Enum current) {
    auto const next{std::to_underlying(current) + 1};
    static constexpr auto MAX{std::to_underlying(MAX_VALUE)};

    using Underlying = std::underlying_type_t<Enum>;
    return (next >= MAX) ? static_cast<Enum>(Underlying{0}) : static_cast<Enum>(next);
}

namespace ml {
inline static constexpr wchar_t MyCharacterLogTag[]{TEXT("MyCharacter")};
}

UCLASS()
class SANDBOX_API AMyCharacter
    : public ACharacter
    , public print_msg_mixin
    , public ml::LogMsgMixin<ml::MyCharacterLogTag>
    , public IDeathHandler
    , public IMaxSpeedChangeListener {
    GENERATED_BODY()
  public:
    AMyCharacter();
  protected:
    virtual void BeginPlay() override;

    // Input
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input)
    TObjectPtr<UInputMappingContext> first_person_context;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input)
    TObjectPtr<UInputAction> move_action;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input)
    TObjectPtr<UInputAction> jump_action;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input)
    TObjectPtr<UInputAction> jetpack_action;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input)
    TObjectPtr<UInputAction> cycle_camera_action;
  public:
    virtual void Tick(float DeltaTime) override;
    virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

    UPROPERTY()
    AMyPlayerController* my_player_controller;
    UPROPERTY()
    bool is_forced_movement;

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

    // Camera
    UPROPERTY(VisibleAnywhere, Category = "Camera")
    UCameraComponent* first_person_camera_component{nullptr};
    UPROPERTY(VisibleAnywhere, Category = "Camera")
    UCameraComponent* third_person_camera_component{nullptr};
    UPROPERTY(VisibleAnywhere, Category = "Camera")
    USpringArmComponent* spring_arm{nullptr};
    UPROPERTY(VisibleAnywhere, Category = "Camera")
    ECharacterCameraMode camera_mode{ECharacterCameraMode::FirstPerson};
    UCameraComponent const* get_active_camera() const;

    // Movement
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement");
    float move_speed{800.0f};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement");
    float acceleration{100.0f};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement");
    UJetpackComponent* jetpack_component;

    // Speed
    UPROPERTY(BlueprintAssignable, Category = "Movement")
    FOnMaxSpeedChanged OnMaxSpeedChanged;

    virtual void on_speed_changed(float new_speed);

    // Torch
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Torch")
    USpotLightComponent* torch_component;
    UFUNCTION()
    void aim_torch(FVector const& world_location);
    UFUNCTION()
    void reset_torch_position();
    UFUNCTION()
    void toggle_torch();
    UFUNCTION()
    void set_torch(bool on);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Abilities")
    UWarpComponent* warp_component{nullptr};
  private:
    bool torch_on{true};
    int32 cached_jump_count{0};
    APlayerController* player_controller{nullptr};
    AMyHUD* hud{nullptr};

    virtual void handle_death();
    void disable_all_cameras();
    void change_camera_to(ECharacterCameraMode mode);
};
