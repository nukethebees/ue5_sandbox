#include "MyCharacter.h"

#include "Components/CapsuleComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sandbox/actor_components/InteractorComponent.h"
#include "Sandbox/actor_components/JetpackComponent.h"
#include "Sandbox/huds/MyHud.h"
#include "Sandbox/interfaces/Interactable.h"
#include "UObject/ScriptInterface.h"

AMyCharacter::AMyCharacter() {
    PrimaryActorTick.bCanEverTick = true;

    first_person_camera_component = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
    first_person_camera_component->bUsePawnControlRotation = true;
    first_person_camera_component->SetupAttachment(RootComponent);

    spring_arm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
    spring_arm->SetupAttachment(RootComponent);

    third_person_camera_component =
        CreateDefaultSubobject<UCameraComponent>(TEXT("ThirdPersonCamera"));
    third_person_camera_component->bUsePawnControlRotation = true;
    third_person_camera_component->SetupAttachment(spring_arm);

    warp_component = CreateDefaultSubobject<UWarpComponent>(TEXT("WarpComponent"));

    torch_component = CreateDefaultSubobject<USpotLightComponent>(TEXT("Torch"));
    torch_component->SetupAttachment(RootComponent);
}

void AMyCharacter::BeginPlay() {
    Super::BeginPlay();

    check(GEngine != nullptr);

    is_forced_movement = false;

    jetpack_component = FindComponentByClass<UJetpackComponent>();

    if (torch_component) {
        torch_component->bCastVolumetricShadow = true;
        torch_component->VolumetricScatteringIntensity = 1.0f;
        torch_component->AttenuationRadius = 2000.0f;
        set_torch(false);
    }

    auto& char_movement{*GetCharacterMovement()};
    char_movement.MaxWalkSpeed = this->move_speed;
    char_movement.MaxAcceleration = this->acceleration;
    OnMaxSpeedChanged.Broadcast(char_movement.MaxWalkSpeed);

    this->JumpMaxCount = 2;

    using PC = std::remove_pointer<decltype(player_controller)>::type;
    player_controller = Cast<PC>(this->Controller);

    if (player_controller) {
        using EIPS = UEnhancedInputLocalPlayerSubsystem;
        auto const* local_player{player_controller->GetLocalPlayer()};

        if (auto* subsystem{ULocalPlayer::GetSubsystem<EIPS>(local_player)}) {
            subsystem->AddMappingContext(this->input_actions.first_person_context, 0);
        }

        using HUD = std::remove_pointer<decltype(hud)>::type;
        hud = Cast<HUD>(player_controller->GetHUD());

        if (hud) {
            hud->update_jump(JumpCurrentCount);
        }
    }
}

void AMyCharacter::Tick(float DeltaTime) {
    Super::Tick(DeltaTime);

    if (cached_jump_count != JumpCurrentCount) {
        if (player_controller) {
            if (hud) {
                hud->update_jump(JumpCurrentCount);
            } else {
                print_msg(TEXT("No HUD when updating HUD."));
            }
        } else {
            print_msg(TEXT("No PC when updating HUD."));
        }

        cached_jump_count = JumpCurrentCount;
    }
}

void AMyCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) {
    if (auto* eic{CastChecked<UEnhancedInputComponent>(PlayerInputComponent)}) {
        eic->BindAction(
            this->input_actions.move_action, ETriggerEvent::Triggered, this, &AMyCharacter::move);

        // Bind Jump Actions
        eic->BindAction(input_actions.jump_action, ETriggerEvent::Started, this, &ACharacter::Jump);
        eic->BindAction(
            input_actions.jump_action, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

        eic->BindAction(input_actions.jetpack_action,
                        ETriggerEvent::Triggered,
                        this,
                        &AMyCharacter::start_jetpack);
        eic->BindAction(input_actions.jetpack_action,
                        ETriggerEvent::Completed,
                        this,
                        &AMyCharacter::stop_jetpack);

        eic->BindAction(input_actions.cycle_camera_action,
                        ETriggerEvent::Started,
                        this,
                        &AMyCharacter::cycle_camera);
    }
}

void AMyCharacter::move(FInputActionValue const& value) {
    auto const movement_value{value.Get<FVector>()};

    if (this->Controller) {
        if (!is_forced_movement) {
            AddMovementInput(GetActorRightVector(), movement_value.X);
            AddMovementInput(GetActorForwardVector(), movement_value.Y);
        }

        // Allow slow vertical motion when in the forced movement state
        // to allow the characters to get unstuck if needed
        auto const z_scale{is_forced_movement ? 0.2f : 1.0f};
        AddMovementInput(GetActorUpVector(), movement_value.Z * z_scale);
    }
}
void AMyCharacter::look(FInputActionValue const& value) {
    auto const look_axis_value{value.Get<FVector2D>()};

    if (Controller) {
        AddControllerYawInput(look_axis_value.X);
        AddControllerPitchInput(look_axis_value.Y);
    }
}
void AMyCharacter::start_jetpack(FInputActionValue const&) {
    if (jetpack_component != nullptr) {
        jetpack_component->start_jetpack();
    }
}
void AMyCharacter::stop_jetpack(FInputActionValue const&) {
    if (jetpack_component != nullptr) {
        jetpack_component->stop_jetpack();
    }
}
void AMyCharacter::cycle_camera(FInputActionValue const&) {
    camera_mode = get_next(camera_mode);
    change_camera_to(camera_mode);
}
UCameraComponent const* AMyCharacter::get_active_camera() const {
    switch (camera_mode) {
        using enum ECharacterCameraMode;

        case FirstPerson: {
            return first_person_camera_component;
        }
        case ThirdPerson: {
            return third_person_camera_component;
        }
        default: {
            break;
        }
    }

    log_warning(TEXT("Unhandled camera mode. Returning first person."));
    return first_person_camera_component;
}

void AMyCharacter::aim_torch(FVector const& world_location) {
    if (!torch_component) {
        print_msg("No torch");
        return;
    }

    auto const location{torch_component->GetComponentLocation()};
    auto const direction{(world_location - location).GetSafeNormal()};
    auto const look_at_rotation{direction.Rotation()};
    torch_component->SetWorldRotation(look_at_rotation);
}
void AMyCharacter::reset_torch_position() {
    if (!torch_component) {
        print_msg("No torch");
        return;
    }

    auto const fwd_rot{GetActorForwardVector().Rotation()};
    torch_component->SetWorldRotation(fwd_rot);
}
void AMyCharacter::toggle_torch() {
    set_torch(!torch_on);
}
void AMyCharacter::set_torch(bool on) {
    if (!torch_component) {
        print_msg("No torch");
        return;
    }

    torch_on = on;
    torch_component->SetVisibility(torch_on);
}

void AMyCharacter::handle_death() {
    if (auto const* world{GetWorld()}) {
        UGameplayStatics::OpenLevel(world, "MainMenu");
    }
}

void AMyCharacter::on_speed_changed(float new_speed) {
    OnMaxSpeedChanged.Broadcast(new_speed);
}
void AMyCharacter::disable_all_cameras() {
    first_person_camera_component->SetActive(false);
    third_person_camera_component->SetActive(false);
}
void AMyCharacter::change_camera_to(ECharacterCameraMode mode) {
    log_verbose(TEXT("Changing camera mode to %s."), *UEnum::GetValueAsString(camera_mode));

    disable_all_cameras();
    switch (camera_mode) {
        using enum ECharacterCameraMode;

        case FirstPerson: {
            first_person_camera_component->SetActive(true);
            break;
        }
        case ThirdPerson: {
            third_person_camera_component->SetActive(true);
            break;
        }
        default: {
            log_warning(TEXT("Unhandled camera mode. Switching to first person."));
            first_person_camera_component->SetActive(true);
        }
    }
}
